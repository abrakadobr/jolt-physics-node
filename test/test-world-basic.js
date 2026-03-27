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

const bm = world.bodiesManager()
console.log({bm})
const box = bm.createBox( {
    halfExtent : {x: 10, y: 0.5, z: 10},
  },
  {
    position: {x: 0, y: -0.5, z: 0},
    rotation: {x: 0, y: 0, z: 0, w: 1},
    activate: true,
    motionType: "static",
    layer: "static"
  }
)
console.log({box})

const sphere = bm.createSphere({
    radius: 1.0
  },{
    position: {x:0, y: 3, z: 0},
    rotation: { x:0, y:0, z:0, w:1},
    active: true,
    motionType: "dynamic",
    layer: "dynamic"
  })
console.log({sphere, box})
sphere.on('transform', data => {
  console.log('O', data)
})
sphere.on('contact-added', other => {
  console.log('0', 'in contact with', other )
})
world.worldRun(1.0, true);

let i =0;
const timer = setInterval(() => {
  console.log({'box': box.position(), " sphere": sphere.position()})
  i++
  if (i > 50) {
    console.log('stopping world')
    clearInterval(timer)
    world.worldStop()
  }
}, 200)

console.log('bm', bm)
