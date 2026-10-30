#!/usr/bin/env python3
"""
FT2 Vita data extractor.

Builds the data folder used by the PS Vita port from YOUR OWN copy of the
Fish Tycoon 2 Android package (.xapk, or base .apk). Nothing from the game is
shipped with the port itself.

usage:  python3 ft2extract.py <Fish_Tycoon_2.xapk | base.apk> <out_dir>
needs:  pip install UnityPy Pillow
optional: ffmpeg on PATH (music is converted to .ogg; otherwise kept as .wav)
"""
import sys, os, io, json, struct, zipfile, math, tempfile, shutil, subprocess, re, collections

import UnityPy
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
SCHEMA = json.load(open(os.path.join(HERE, 'schema.json')))
SCALE = 0.5            # 1080p art -> 544p Vita screen
ATLAS = 2048

# ---------------------------------------------------------------- schema reader
BUILTIN_FMT = {'2f': '<2f', '3f': '<3f', '4f': '<4f', 'I': '<I', '2i': '<2i', '3i': '<3i', '6f': '<6f', '4i': '<4i'}


def _al(o):
    return (o + 3) & ~3


def read_val(d, o, t):
    if isinstance(t, str):
        if t == 'bool': return bool(d[o]), _al(o + 1)
        if t == 'u8': return d[o], _al(o + 1)
        if t == 'u16': return struct.unpack_from('<H', d, o)[0], _al(o + 2)
        if t == 'i32': return struct.unpack_from('<i', d, o)[0], o + 4
        if t == 'u32': return struct.unpack_from('<I', d, o)[0], o + 4
        if t == 'i64': return struct.unpack_from('<q', d, o)[0], o + 8
        if t == 'f32': return struct.unpack_from('<f', d, o)[0], o + 4
        if t == 'f64': return struct.unpack_from('<d', d, o)[0], o + 8
        if t == 'str':
            n = struct.unpack_from('<i', d, o)[0]
            return d[o + 4:o + 4 + n].decode('utf8', 'replace'), _al(o + 4 + n)
        if t == 'ptr':
            fid, pid = struct.unpack_from('<iq', d, o)
            return (fid, pid), o + 12
        if t.startswith('B:'):
            f = BUILTIN_FMT[t[2:]]
            return list(struct.unpack_from(f, d, o)), o + struct.calcsize(f)
        if t == 'curve':
            n = struct.unpack_from('<i', d, o)[0]; o += 4 + 28 * n + 12
            return None, o
        if t == 'gradient':
            return None, o + 8 * 16 + 8 * 2 + 8 * 2 + 4
        raise ValueError(t)
    if isinstance(t, list):
        n = struct.unpack_from('<i', d, o)[0]; o += 4
        et = t[1]
        if et in ('bool', 'u8'):
            return list(d[o:o + n]), _al(o + n)
        out = []
        for _ in range(n):
            v, o = read_val(d, o, et); out.append(v)
        return out, _al(o)
    return read_obj(d, o, t['c'])


def read_obj(d, o, cls):
    r = {}
    for name, t in SCHEMA[cls]:
        r[name], o = read_val(d, o, t)
    return r, o


def read_mono(raw, cls):
    o = 28
    n = struct.unpack_from('<i', raw, o)[0]
    o = _al(o + 4 + n)
    v, o = read_obj(raw, o, cls)
    return v


# ---------------------------------------------------------------- environment
class Game:
    def __init__(self, data_dir):
        self.env = UnityPy.load(data_dir)
        self.objs = {(o.assets_file.name, o.path_id): o for o in self.env.objects}
        self.scr = {}
        for o in self.env.objects:
            if o.type.name == 'MonoScript':
                d = o.read()
                self.scr[(o.assets_file.name, o.path_id)] = (d.m_Namespace + '.' if d.m_Namespace else '') + d.m_ClassName
        self.sprites = {}      # key -> info
        self._tex = {}

    def resolve(self, f, fid, pid):
        if pid == 0: return None
        fname = f.name if fid == 0 else f.externals[fid - 1].path.split('/')[-1]
        return self.objs.get((fname, pid))

    def rp(self, o, ptr):
        return self.resolve(o.assets_file, ptr[0], ptr[1])

    def script_of(self, o):
        raw = o.get_raw_data()
        sfid, spid = struct.unpack_from('<iq', raw, 16)
        f = o.assets_file
        fname = f.name if sfid == 0 else f.externals[sfid - 1].path.split('/')[-1]
        return self.scr.get((fname, spid), '?')

    def mono(self, o):
        s = self.script_of(o)
        if s not in SCHEMA: return s, None
        try:
            return s, read_mono(o.get_raw_data(), s)
        except Exception:
            return s, None

    def tex_image(self, to):
        k = (to.assets_file.name, to.path_id)
        if k not in self._tex:
            self._tex[k] = to.read().image.convert('RGBA')
        return self._tex[k]

    def sprite(self, so):
        """register a Sprite object, return key"""
        if so is None: return None
        k = '%s:%d' % (so.assets_file.name, so.path_id)
        if k in self.sprites: return k
        s = so.read()
        t = s.m_RD.texture
        to = self.resolve(so.assets_file, t.m_FileID, t.m_PathID)
        tr = s.m_RD.textureRect
        off = s.m_RD.textureRectOffset
        self.sprites[k] = dict(name=s.m_Name, texobj=to, trect=[tr.x, tr.y, tr.width, tr.height],
                               off=[off.x, off.y], rect=[s.m_Rect.width, s.m_Rect.height],
                               pivot=[s.m_Pivot.x, s.m_Pivot.y],
                               border=[s.m_Border.x, s.m_Border.y, s.m_Border.z, s.m_Border.w],
                               ppu=s.m_PixelsToUnits)
        return k

    def go_of(self, comp_obj):
        raw = comp_obj.get_raw_data()
        fid, pid = struct.unpack_from('<iq', raw, 0)
        return self.resolve(comp_obj.assets_file, fid, pid)

    def components(self, go_obj):
        go = go_obj.read()
        out = []
        for c in go.m_Component:
            cp = c.component if hasattr(c, 'component') else c
            co = self.resolve(go_obj.assets_file, cp.m_FileID, cp.m_PathID)
            if co is not None: out.append(co)
        return go, out

    def transform_of(self, go_obj):
        go, comps = self.components(go_obj)
        for co in comps:
            if co.type.name in ('Transform', 'RectTransform'): return co
        return None


# ---------------------------------------------------------------- node export
def col(c):
    return [round(float(x), 4) for x in c]


def export_node(G, to, ui=True):
    t = to.read()
    go_obj = G.resolve(to.assets_file, t.m_GameObject.m_FileID, t.m_GameObject.m_PathID)
    go, comps = G.components(go_obj)
    n = {'n': go.m_Name}
    if not go.m_IsActive: n['off'] = 1
    if to.type.name == 'RectTransform':
        n['r'] = [round(v, 3) for v in (t.m_AnchorMin.x, t.m_AnchorMin.y, t.m_AnchorMax.x, t.m_AnchorMax.y,
                                         t.m_AnchoredPosition.x, t.m_AnchoredPosition.y, t.m_SizeDelta.x, t.m_SizeDelta.y,
                                         t.m_Pivot.x, t.m_Pivot.y)]
    p = t.m_LocalPosition
    n['p'] = [round(p.x, 3), round(p.y, 3), round(p.z, 3)]
    s = t.m_LocalScale
    if (s.x, s.y) != (1.0, 1.0): n['s'] = [round(s.x, 4), round(s.y, 4)]
    q = t.m_LocalRotation
    rz = math.degrees(2 * math.atan2(q.z, q.w))
    if abs(rz) > 0.01: n['rz'] = round(rz, 2)
    cl = []
    for co in comps:
        tn = co.type.name
        if tn in ('Transform', 'RectTransform', 'CanvasRenderer'): continue
        if tn == 'SpriteRenderer':
            d = co.read()
            sk = G.sprite(G.resolve(co.assets_file, d.m_Sprite.m_FileID, d.m_Sprite.m_PathID))
            if sk:
                cl.append({'t': 'sr', 'sp': sk, 'c': col((d.m_Color.r, d.m_Color.g, d.m_Color.b, d.m_Color.a)),
                           'o': d.m_SortingOrder, 'fx': int(d.m_FlipX), 'fy': int(d.m_FlipY)})
            continue
        if tn == 'CanvasGroup':
            cl.append({'t': 'cg', 'a': round(co.read().m_Alpha, 3)}); continue
        if tn != 'MonoBehaviour':
            continue
        sname, v = G.mono(co)
        if v is None:
            cl.append({'t': sname}); continue
        if sname == 'UnityEngine.UI.Image':
            sk = G.sprite(G.rp(co, v['m_Sprite']))
            e = {'t': 'img', 'c': col(v['m_Color']), 'ty': v['m_Type'], 'ray': int(v['m_RaycastTarget'])}
            if sk: e['sp'] = sk
            if v['m_Type'] == 3: e.update(fa=round(v['m_FillAmount'], 3), fm=v['m_FillMethod'], fo=v['m_FillOrigin'])
            if v['m_PreserveAspect']: e['pa'] = 1
            if not v['m_FillCenter']: e['nofill'] = 1
            cl.append(e)
        elif sname == 'UnityEngine.UI.Text':
            fd = v['m_FontData']
            fo = G.rp(co, fd['m_Font'])
            cl.append({'t': 'txt', 'tx': v['m_Text'], 'c': col(v['m_Color']), 'f': fo.read().m_Name if fo else 'Arial',
                       'sz': fd['m_FontSize'], 'st': fd['m_FontStyle'], 'bf': int(fd['m_BestFit']),
                       'mn': fd['m_MinSize'], 'mx': fd['m_MaxSize'], 'al': fd['m_Alignment'],
                       'ho': fd['m_HorizontalOverflow'], 'vo': fd['m_VerticalOverflow'], 'ls': round(fd['m_LineSpacing'], 3)})
        elif sname in ('UnityEngine.UI.Outline', 'UnityEngine.UI.Shadow', 'ToJShadow'):
            cl.append({'t': 'outline' if sname.endswith('Outline') else 'shadow', 'c': col(v['m_EffectColor']),
                       'd': [round(x, 2) for x in v['m_EffectDistance']]})
        elif sname == 'UnityEngine.UI.Button':
            cl.append({'t': 'btn', 'i': int(v['m_Interactable'])})
        elif sname == 'UnityEngine.UI.Toggle':
            cl.append({'t': 'toggle', 'on': int(v.get('m_IsOn', 0))})
        elif sname in ('UnityEngine.UI.GridLayoutGroup',):
            cl.append({'t': 'grid', 'pad': v['m_Padding'], 'cell': v['m_CellSize'], 'sp': v['m_Spacing'],
                       'ca': v['m_ChildAlignment'], 'con': v['m_Constraint'], 'cc': v['m_ConstraintCount'],
                       'ax': v['m_StartAxis']})
        elif sname in ('UnityEngine.UI.HorizontalLayoutGroup', 'UnityEngine.UI.VerticalLayoutGroup'):
            cl.append({'t': 'hlg' if 'Horizontal' in sname else 'vlg'})
        elif sname == 'UnityEngine.UI.RectMask2D':
            cl.append({'t': 'mask'})
        elif sname == 'UnityEngine.UI.ScrollRect':
            cl.append({'t': 'scroll', 'h': int(v['m_Horizontal']), 'v': int(v['m_Vertical'])})
        else:
            small = {k: x for k, x in v.items() if isinstance(x, (int, float, str, bool))}
            e = {'t': sname}
            if small: e['v'] = small
            cl.append(e)
    if cl: n['c'] = cl
    kids = []
    for k in t.m_Children:
        ko = G.resolve(to.assets_file, k.m_FileID, k.m_PathID)
        if ko is not None: kids.append(export_node(G, ko, ui))
    if kids: n['k'] = kids
    return n


def find_root_transform(G, scene, name):
    for (fn, pid), o in G.objs.items():
        if fn != scene or o.type.name not in ('Transform', 'RectTransform'): continue
        t = o.read()
        go = G.resolve(o.assets_file, t.m_GameObject.m_FileID, t.m_GameObject.m_PathID).read()
        if go.m_Name == name: return o
    return None


def export_prefab(G, go_obj):
    to = G.transform_of(go_obj)
    return export_node(G, to, False)


# ---------------------------------------------------------------- atlas packing
def sprite_pixels(G, info):
    tex = G.tex_image(info['texobj'])
    x, y, w, h = info['trect']
    th = tex.size[1]
    box = (int(round(x)), int(round(th - y - h)), int(round(x + w)), int(round(th - y)))
    return tex.crop(box)


def pack_sprites(G, out_dir):
    items = []
    for k, info in G.sprites.items():
        if info['texobj'] is None: continue
        im = sprite_pixels(G, info)
        w = max(1, int(round(im.size[0] * SCALE))); h = max(1, int(round(im.size[1] * SCALE)))
        if (w, h) != im.size: im = im.resize((w, h), Image.LANCZOS)
        items.append((k, im))
    # big images (backgrounds) go to their own file
    singles = [(k, im) for k, im in items if im.size[0] > 512 or im.size[1] > 512]
    small = [(k, im) for k, im in items if not (im.size[0] > 512 or im.size[1] > 512)]
    small.sort(key=lambda t: (-t[1].size[1], -t[1].size[0]))
    atlases = []; meta = {}
    cur = None; cx = cy = rowh = 0
    PAD = 2

    def new_atlas():
        atlases.append(Image.new('RGBA', (ATLAS, ATLAS), (0, 0, 0, 0)))
        return 0, 0, 0
    cx, cy, rowh = new_atlas()
    for k, im in small:
        w, h = im.size
        if cx + w + PAD > ATLAS:
            cx = 0; cy += rowh + PAD; rowh = 0
        if cy + h + PAD > ATLAS:
            cx, cy, rowh = new_atlas()
        atlases[-1].paste(im, (cx, cy))
        meta[k] = ['a%d' % (len(atlases) - 1), cx, cy, w, h]
        cx += w + PAD; rowh = max(rowh, h)
    for i, a in enumerate(atlases):
        a.save(os.path.join(out_dir, 'gfx', 'a%d.png' % i), optimize=True)
    for i, (k, im) in enumerate(singles):
        fn = 'b%d' % i
        if im.getextrema()[3][0] == 255:
            im.convert('RGB').save(os.path.join(out_dir, 'gfx', fn + '.jpg'), quality=90)
            fn_full = fn + '.jpg'
        else:
            im.save(os.path.join(out_dir, 'gfx', fn + '.png'), optimize=True)
            fn_full = fn + '.png'
        meta[k] = [fn_full, 0, 0, im.size[0], im.size[1]]
    sprites = {}
    for k, info in G.sprites.items():
        if k not in meta: continue
        a, x, y, w, h = meta[k]
        rw, rh = info['rect']
        # placement of trimmed texture rect inside full sprite rect (unity: offset from bottom-left)
        sprites[k] = {'n': info['name'], 'a': a, 'x': x, 'y': y, 'w': w, 'h': h,
                      'fw': round(rw * SCALE, 2), 'fh': round(rh * SCALE, 2),
                      'ox': round(info['off'][0] * SCALE, 2), 'oy': round(info['off'][1] * SCALE, 2),
                      'px': round(info['pivot'][0], 4), 'py': round(info['pivot'][1], 4),
                      'b': [round(v * SCALE, 2) for v in info['border']], 'ppu': info['ppu']}
    return sprites, len(atlases)


# ---------------------------------------------------------------- main
def main():
    src, out = sys.argv[1], sys.argv[2]
    work = tempfile.mkdtemp(prefix='ft2x_')
    try:
        base = src
        if src.lower().endswith('.xapk'):
            z = zipfile.ZipFile(src)
            names = [n for n in z.namelist() if n.endswith('.apk')]
            bn = [n for n in names if 'config.' not in n][0]
            z.extract(bn, work); base = os.path.join(work, bn)
        print('[1/7] unpacking game data ...')
        z = zipfile.ZipFile(base)
        for n in z.namelist():
            if n.startswith('assets/bin/Data/') and '/Managed/' not in n:
                z.extract(n, work)
        data_dir = os.path.join(work, 'assets', 'bin', 'Data')
        for sub in ('gfx', 'audio', 'fonts', 'cfg'):
            os.makedirs(os.path.join(out, sub), exist_ok=True)
        print('[2/7] loading unity assets (this takes a minute) ...')
        G = Game(data_dir)

        # ---------- scripts by name
        by_script = collections.defaultdict(list)
        for (fn, pid), o in G.objs.items():
            if o.type.name == 'MonoBehaviour':
                by_script[G.script_of(o)].append(o)

        print('[3/7] fish ...')
        fed = read_mono(by_script['FishEditorData'][0].get_raw_data(), 'FishEditorData')
        fp_mb = by_script['FishPrefabs'][0]
        fpv = read_mono(fp_mb.get_raw_data(), 'FishPrefabs')
        repo_o = G.rp(fp_mb, fpv['m_fishPrefabsRepo'])
        repo = read_mono(repo_o.get_raw_data(), 'FishPrefabsRepo')

        def part(ptr, owner):
            go_obj = G.rp(owner, ptr)
            if go_obj is None: return None
            return export_prefab(G, go_obj)
        species = []
        for f in repo['Fish']:
            sp = {'name': f['FishName'], 'type': f['TypeIndex'], 'frames': []}
            for i in range(8):
                fr = {}
                for key, arr in (('body', 'Bodies'), ('top', 'TopFins'), ('bottom', 'BottomFins'), ('side', 'SideFins'), ('tail', 'Tails')):
                    lst = f[arr]
                    if not lst: continue
                    ptr = lst[i] if i < len(lst) else lst[0]
                    if key == 'tail' and i > 0 and len(lst) == 1: continue
                    fr[key] = part(ptr, repo_o)
                sp['frames'].append(fr)
            species.append(sp)
        fins = {}
        for fish in fed['Fish']:
            fins[fish['Name']] = {fin['Name']: [[fr['TopFin']['Position'][:2] + [fr['TopFin']['Rotation'][2]] + fr['TopFin']['Scale'][:2],
                                                  fr['BottomFin']['Position'][:2] + [fr['BottomFin']['Rotation'][2]] + fr['BottomFin']['Scale'][:2],
                                                  fr['SideFin']['Position'][:2] + [fr['SideFin']['Rotation'][2]] + fr['SideFin']['Scale'][:2]]
                                                 for fr in fin['Frames']] for fin in fish['Fins']}
        # fish prefab (dead sprite, emitters) – keep sprite refs only
        json.dump({'species': species, 'finFrames': fins}, open(os.path.join(out, 'cfg', 'fish.json'), 'w'), separators=(',', ':'))

        print('[4/7] items, tanks, ui ...')
        iv = read_mono(by_script['InventoryItemRepo'][0].get_raw_data(), 'InventoryItemRepo')
        iro = by_script['InventoryItemRepo'][0]
        items = []
        for it in iv['m_items']:
            e = {'id': it['Id'], 'icon': G.sprite(G.rp(iro, it['ItemSprite']))}
            pf = G.rp(iro, it['PrefabForTank'])
            if pf is not None:
                e['prefab'] = export_prefab(G, pf)
            items.append(e)
        json.dump(items, open(os.path.join(out, 'cfg', 'items.json'), 'w'), separators=(',', ':'))

        tank = export_node(G, find_root_transform(G, 'level1', 'TankSystem'), False)
        json.dump(tank, open(os.path.join(out, 'cfg', 'tank.json'), 'w'), separators=(',', ':'))
        store = export_node(G, find_root_transform(G, 'level4', 'FishStore'), False)
        json.dump(store, open(os.path.join(out, 'cfg', 'fishstore.json'), 'w'), separators=(',', ':'))
        ui = export_node(G, find_root_transform(G, 'level0', 'CanvasRoot'), True)
        json.dump(ui, open(os.path.join(out, 'cfg', 'ui.json'), 'w'), separators=(',', ':'))
        # runtime-instantiated UI prefabs referenced by UI scripts (store items, inventory cells, species cells…)
        prefabs = {}
        scan = []
        for sname, lst in by_script.items():
            for o in lst:
                if o.assets_file.name == 'level0': scan.append(o)
        if True:
            for o in scan:
                raw = o.get_raw_data()
                # scan raw data for PPtrs to prefabs (GameObjects outside scene)
                for off in range(28, len(raw) - 12, 4):
                    fid, pid = struct.unpack_from('<iq', raw, off)
                    if 0 < fid <= len(o.assets_file.externals) and 0 < pid < 1 << 40:
                        tgt = G.resolve(o.assets_file, fid, pid)
                        if tgt is not None and tgt.type.name == 'GameObject':
                            nm = tgt.read().m_Name
                            if nm not in prefabs and nm not in ('Fish',):
                                try: prefabs[nm] = export_prefab(G, tgt)
                                except Exception: pass
        json.dump(prefabs, open(os.path.join(out, 'cfg', 'uiprefabs.json'), 'w'), separators=(',', ':'))

        # extra loose sprites we draw directly
        for o in G.env.objects:
            if o.type.name == 'Sprite':
                nm = o.read().m_Name
                if nm in ('Dead_Fish', 'babyfish', 'BubbleParticle', 'Sparkle_Particle', 'coin_hud', 'gem_hud',
                          'icon_priceoutrage', 'icon_sold', 'caustics', 'logo', 'Woman', 'Man', 'character', 'character2',
                          'ThoughtBubble', 'shadow', 'closedshell', 'openshell', 'icon_coin_fishinfo_price', 'fishselect_corner') \
                        or nm.startswith(('animation_coin', 'Alive_', 'Dead_0', 'research_icon', 'upgrade', 'iap_icon_', 'supplies_', 'banner_', 'holder_powerup', 'reward(', 'chest')):
                    G.sprite(o)
        print('[5/7] packing %d sprites ...' % len(G.sprites))
        sprites, na = pack_sprites(G, out)
        json.dump(sprites, open(os.path.join(out, 'cfg', 'sprites.json'), 'w'), separators=(',', ':'))
        print('      %d atlases' % na)

        print('[6/7] text data & fonts ...')
        for o in G.env.objects:
            if o.type.name == 'TextAsset':
                d = o.read()
                if d.m_Name in ('Tasks', 'FishStore', 'Audio', 'MaleBody', 'FemaleBody', 'MascotBody', 'MaleBody.atlas', 'FemaleBody.atlas', 'MascotBody.atlas'):
                    s = d.m_Script
                    b = s.encode('utf-8', 'surrogateescape') if isinstance(s, str) else bytes(s)
                    open(os.path.join(out, 'cfg', d.m_Name + '.txt'), 'wb').write(b)
            elif o.type.name == 'Font':
                d = o.read()
                if d.m_FontData:
                    open(os.path.join(out, 'fonts', d.m_Name + '.ttf'), 'wb').write(bytes(d.m_FontData))
            elif o.type.name == 'Texture2D':
                d = o.read()
                if d.m_Name in ('MaleBody', 'FemaleBody', 'MascotBody'):
                    im = d.image
                    im.save(os.path.join(out, 'gfx', 'spine_' + d.m_Name + '.png'))
        print('[7/7] audio ...')
        ff = shutil.which('ffmpeg')
        for o in G.env.objects:
            if o.type.name != 'AudioClip': continue
            d = o.read()
            try:
                samples = d.samples
            except Exception as e:
                print('  skip', d.m_Name, e); continue
            for name, data in samples.items():
                base_name = os.path.splitext(name)[0]
                wav = os.path.join(work, base_name + '.wav')
                open(wav, 'wb').write(data)
                is_music = 'music' in base_name.lower()
                if ff:
                    if is_music:
                        dst = os.path.join(out, 'audio', base_name + '.ogg')
                        subprocess.run([ff, '-y', '-loglevel', 'error', '-i', wav, '-ac', '2', '-ar', '44100', '-c:a', 'libvorbis', '-q:a', '4', dst])
                    else:
                        dst = os.path.join(out, 'audio', base_name + '.wav')
                        subprocess.run([ff, '-y', '-loglevel', 'error', '-i', wav, '-ac', '1', '-ar', '22050', '-sample_fmt', 's16', dst])
                else:
                    shutil.copy(wav, os.path.join(out, 'audio', base_name + '.wav'))
        open(os.path.join(out, 'VERSION'), 'w').write('ft2vita-data 1\n')
        print('done ->', out)
    finally:
        shutil.rmtree(work, ignore_errors=True)


if __name__ == '__main__':
    main()
