# Fish Tycoon 2 – recovered logic (from IL2CPP arm64 disassembly, v1.11.6)

## GameSettings (consts/statics)
startingCoins 500, startingGems 100, secondsPerAge 450, ageUntilTeen 6, ageUntilAdult 20,
agesOfPregnancy 4, maxHealth 300, overfedPenalty 6, starvationPenalty 3, sicknessPenalty 1,
medicineBaseEffect 60, medicineRandomEffect 40, magicFishOfHealthBenefit 3,
temperatureRegulator/aeration/powerFilter: chance 40, benefit 3,
environmentEffectOn{Common,Uncommon,Rare} by envLevel-1: [3,9,12,15] [-3,3,6,9] [-57,-18,-9,0]
foodEffectOn{Common,Uncommon,Rare} (infestation chance base by foodLevel-1): [10,5,3,1] [20,10,6,3] [30,20,10,5]
infestationChanceRange 10000, spread +6 per infested tankmate, oldAge>600 +15, plantOfCuring /2, cleaningSnail 50% → /2
breedingDuration [3,3,3] seconds (allure potion => 0), allurePotionDuration 300
tankCapacity 21 (per tank, GetCapacityForTankIndex from TankData)
costToReviveFish 2 (gems)
FishData ctor: health=300, food=90, infested=0, age 0.

## Rarity of part index p (0..20): odd → rare(2); p%4==2 → uncommon(1); p%4==0 → common(0).
Fish rarity = max(body rarity, fin rarity).

## Magic fish (body,fin) → kind (enum index 0-based):
Fertility(9,7)=0 Sustenance(8,12)=1 Curing(18,15)=2 Growth(15,8)=3 Health(6,2)=4 Mutation(19,3)=5 GoldenGuppy(7,13)=6

## UpdateAllFish (per fish, on each tick)
secondsAccumulated = min(now - lastTimeChecked + secondsAccumulated, 259200)
if paused: agesChecked=age, secondsAccumulated=0 else ages = secondsAccumulated / 450
lastTimeChecked=now
if Growth magic in tank and secondsAccumulated>=271: per age 11% chance extra age
age += ages; secondsAccumulated -= ages*450
if newly adult (age>=20, prev<20) → discover species (breed price default), magic discovered event
if pregnant (ageToBirth!=0 && age>=ageToBirth):
   if Mutation magic in tank: 16% chance → 50%: mutate baby fin ±1 (else body ±1), clamp [0,20]
   SpawnBabyFish once, again if Fertility magic in tank, again if 'plant of fertility'
   ageToBirth = 0
for each age step k (agesChecked < age) while health>0:
   HealthTax(); if Health magic: health += 3
   if age>=20: ChanceOfInfestation()
   if infested: if Curing magic: 20% cure. else if pregnant: miscarry (ageToBirth=0)
   food decay: foodLevel 1: food-=1; 2: 61%; 3: 46%; 4: 31% chance
   if food<=2: food=0; unless Sustenance magic: health -= 3 (starvation)
   plant of nutrition: 21% food+=3; auto feeder: 41% food+=6
   clamp health [0,300], food [0,100]; if health<=0 → dead
## HealthTax
effect = envEffect[rarity][envLevel-1]; if effect<0 environmentDamage -= effect
temp regulator/aeration/power filter each 41%: effect += 3
infested: delta = min(effect,0) - 1, damage counters += 1
else food<=4: delta = min(effect,0)... (only negatives) ; else delta = effect
health = clamp(health+delta, 0, 300)
## ChanceOfInfestation: chance = foodEffect[rarity(body)][foodLevel-1] + 6*infestedTankmates + (age>600?15:0);
plant of curing /2; snail 51%: /2; Random(0,10000) < chance → infested = 1 (fungal) or 2 (ick)
## Breeding: two fish in breeding tank (not both pregnant) → after duration: mother.ageToBirth = age+4;
baby body=(m.body+f.body)/2, fin=(m.fin+f.fin)/2 (C# int division); father returns to its tank.
## SpawnBabyFish: if tank count < capacity: new fish(baby body/fin), age = mother.age - mother.ageToBirth; else miscarry.
## Eggs: count = Random(0,2) + 3 (+1 egg fertility plant)
common: body,fin = 4*Random(0,6) ; uncommon: 4*Random(0,5)+2 ; rare: 2*Random(0,10)+1
## Items: food +50 each fish (cap 100; overfed: -6 health); medicine: +60+R(0,40) health;
Instagrow: juveniles age = 20+R(0,5), agesChecked=age-3; Instabirth: pregnant ageToBirth=age;
Ick/Fungal treatment: cure that infection (~60%), multi-dose.
## Server-only (not in APK): FishDefs names/values, StoreItems catalog → defined in our gameconfig.
