const Jolt = require('../jolt.js')

console.log('Jolt', Jolt)

const world = new Jolt.World();
// console.log('world', world, world.initialize)
world.initialize({
  memoryPreallocatedMb: 10,
  maxBodies: 10000,
  maxBodyMutexes: 0,
  maxBodiesPairs: 10000,
  gravity: 9.8
})
console.log('world', world)
console.log('world layers', world.layers())

const lm = world.layersManager()

console.log('lm', lm, lm.layersNumber())

const ids = lm.layersIDs()

console.log('ids', ids)
lm.addLayer("generic")
const lm2 = world.layersManager()

const ids2 = lm2.layersIDs()
ids2.forEach(id => {
  console.log('2', id, lm.layerName(id))
})

console.log('world layers', world.layers(), ids2)
