const World = require('../src/js/world.js')
const log = World.log('test 01')

const world = new World()

log.log('world', { world, World })

const go = async () => {
  const initOk = await world.init()
  if (!initOk) {
    log.error('init failed')
    return
  }
  log.success('init done')
  const startOk = await world.start()
  log.info('start reslult', startOk)
  const shutdownOk = await world.shutdown()
  log.info('shutdown reslult', shutdownOk)
}

go().then(() => {
  log.success('test complete')
}).catch(err => {
  log.error('test failed', err)
})
