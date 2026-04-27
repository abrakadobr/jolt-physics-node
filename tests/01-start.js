const World = require('../src/js/world.js')
const JoltServer = require('./server.js')
const log = World.log(':01')

const world = new World()
const server = new JoltServer(world)

log.info('world')

const waitms = ms => new Promise(resolve => setTimeout(resolve, ms))

const go = async () => {
  await server.start({
    public: './html/public',
    index: '01.html'
  })
  const initOk = await world.init()
  if (!initOk) {
    log.error('init failed')
    return
  }
  log.success('init done')
  // await world.start()
  // log.success('start done')
  // const startOk = await world.start()
  // log.info('start reslult', startOk)
  await world.start()
  log.info('waiting 10')
  await waitms(10000)
  log.success('waiting 10')

  const bi = world.bodyInterface();
  const box = await bi.createBox(
    {x: 10, y: 1, z: 10},
    {x: 0, y: -0.5, z: 0},
    {x: 0, y: 0, z: 0, w: 1},
    "static",
    0,
    true,
    true
  )
  if (!box) {
    log.error('box creation failed')
    return
  }
  box.setLogName('box')
  // await box.add()
  // await waitms(2000)
  // await box.activate()
  const sphere = await bi.createSphere(
    1.0,
    {x: 0, y: 5, z: 0},
    {x: 0, y: 0, z: 0, w: 1},
    "dynamic",
    1,
    true,
    true
  )
  log.info('box', box.id())
  log.info('sphere', sphere.id())
  sphere.setLogName('sphere')
  // await bi.activateBody(sphere.id())
  /*
  box.on('added', () => {
    log.warn('@subscription to box: added' )
  })
  box.on('removed', () => {
    log.warn('@subscription to box: removed')
  })
  box.on('activated', () => {
    log.warn('@subscription to box: activated')
  })
  box.on('deactivated', () => {
    log.warn('@subscription to box: deactivated')
  })
  box.on('destroyed', () => {
    log.warn('@subscription to box: destroyed')
  })
  */

  log.success('start done')
  await waitms(2000)
  // await box.remove()
  await waitms(2000)
  /*
  */

}

go().then(() => {
  log.success('test complete')
  setTimeout(() => {
    world.shutdown().then(shutdownOk => {
      log.info('shutdown reslult', shutdownOk)
    })
  }, 3000)
}).catch(err => {
  log.error('test failed', err)
})
