// const EventEmitter = require('events')
const { onEvent, sendCommand } = require('../../jolt.js')
const Log = require('./log.js')
const BodyInterface = require('./bodyInterface.js')
const LEE = require('./logee.js')

// const log = Log.m('World')

// log.log('addon', { onEvent, sendCommand })

class World extends LEE {

  static log() { return Log }

  constructor() {
    super()
    this.setLogName('World')
    this._cid = BigInt(1);
    onEvent(e => {
      this.processEvents(e)
    })
    this._waiters = {}
    this._acks = {}
    this._bi = new BodyInterface(this)
  }

  init(config = {}) {
    return new Promise(resolve => {
      if (this.cmdWait('init', resolve)) return
      const cfg = {
        gravity: 9.8,
        memoryPreallocatedMb: 10,
        maxBodies: 65535,
        numBodyMutexes: 0,
        maxBodiesPairs: 65535,
        maxContacts: 10240,
        ...(config || {})
      }
      this.exec({
        cmd: "init",
        config: cfg
      })
    })
  }

  start() {
    return new Promise(resolve => {
      if (this.cmdWait('start', resolve)) return
      this.exec({ cmd: "start" })
    })
  }

  shutdown() {
    return new Promise(resolve => {
      if (this.cmdWait('shutdown', resolve)) return
      this.exec({ cmd: "shutdown" })
    })
  }

  cmdWait(key, rs) {
    if (!this._waiters[key])
      this._waiters[key] = []
    this._waiters[key].push(rs)
    return this._waiters[key].length > 1
  }

  exec(cmd, ack = null) {
    cmd.commandId = this._cid++
    // this.L().info('cmd', cmd, ack)
    if (ack) {
      this._acks[`${cmd.commandId}`] = ack
    }
    sendCommand(cmd)
    return cmd.commandId
  }

  processEvents(list) {
    // this.L().info('process events', list)
    list.forEach(e => {
      if (e.type === 'init') {
        this.eventWait(e.type, e.success)
        return
      }
      if (e.type === 'start') {
        this.eventWait(e.type, e.success)
        return
      }
      if (e.type === 'shutdown') {
        this.eventWait(e.type, e.success)
        return
      }
      if (['bodyCreated', 'bodyAdded', 'bodyRemoved', 'bodyActivated', 'bodyDeactivated', 'bodyDestroyed'].includes(e.type)) {
        this._bi.processEvent(e)
        return
      }
      this.L().info('process event', e)
      const ack = `${e.commandId}`
      if (this._acks[ack] && typeof this._acks[ack] === 'function') {
        this._acks[ack](e)
        delete this._acks[ack]
      }
    })
  }

  eventWait(key, res) {
    (this._waiters[key] || []).forEach(w => {
      w(res)
    })
    this._waiters[key] = null
  }

  bodyInterface() {
    return this._bi
  }
}

module.exports = World
