import { Manager } from 'socket.io-client'
import { DropABallTest } from 'tests/dropABall.js'
import EE from './ee.js'


export default class Core extends EE {

  constructor() {
    super()
    this._worldState = 'stop'
    this._engineBodies = {}
    this._sceneBodies = {}
    this._viewer = null
    this._io = new Manager( window.location.origin, {
      transports: ['websocket'],
      autoConnect: false
    } )
    this._tests = {}
    this._connected = false
    this._waiters = []
    this._test = null

    this.addTest(DropABallTest)
  }

  boot(viewer) {
    this._viewer = viewer
    this._viewer.core(this)
    const tests = Object.keys(this._tests).reduce((acc, key) => {
      const info = this._tests[key].info()
      acc[key] = info
      return acc
    }, {})
    this._viewer.setTests(tests)
  }

  connected() {
    return this._connected
  }

  connect() {
    return new Promise((resolve, reject) => {
      let resolved = false
      // this._io = io()
      this._socket = this._io.socket('/')
      this._socket.on('connect', socket => {
        console.log('connected!. socket', socket)
        this._connected = true
        if (!resolved)
          resolve()
        resolved = true
        this._viewer.ioConnected()
        while( this._waiters.length) {
          const w = this._waiterts.shift()
          w.resolve()
        }
      })

      this._socket.on('disconnect', () => {
        if (!resolved)
          reject()
        this._connected = false
        resolved = true
        this._viewer.ioDisconnected()
        while( this._waiters.length) {
          const w = this._waiterts.shift()
          w.reject()
        }
      })

      this._socket.on('world:reset', () => {
        console.log('io@world:reset')
      })

      this._socket.connect()
    })
  }

  addTest(Test) {
    const info = Test.info()
    this._tests[info.code] = Test
  }

  wait4connection() {
    return new Promise((resolve, reject) => {
      if (this._connected) return resolve()
      this._waiters.push({ resolve, reject })
    })
  }

  async testSelected(code, sidebar) {
    if (!this._tests[code]) return
    if (this._test) {
      this._test.abortAndClose()
      delete this._test
      this._test = null
    }
    await this.wait4connection()
    this._test = new this._tests[code](this, this._viewer, sidebar)
    this._viewer.setTest(this._test)
    this._test.load(sidebar)
  }

  async resetTest() {
    this._test.reset()
    const resetResult = await this._socket.emitWithAck('world:reset')
    console.log('resetResult', resetResult)
  }
}
