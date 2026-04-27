import { Manager } from 'socket.io-client'
// import { DropABallTest } from 'tests/dropABall.js'
import EE from './ee.js'
import { WorldIO } from './world.io.js'

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
    this._wio = new WorldIO(this)
    // this.addTest(DropABallTest)
  }

  boot(viewer) {
    this._viewer = viewer
    this._viewer.core(this)
    const tests = Object.keys(this._tests).reduce((acc, key) => {
      const info = this._tests[key].info()
      acc[key] = info
      return acc
    }, {})
    // this._viewer.setTests(tests)
    this._wio.on('body:created', (body) => {
      console.log('core::@body:created', body)
      this._viewer.ioBodyCreated(body)
    })
    this._wio.on('body:transform', data => {
      this._viewer.transformBody(data)
    })
    this._wio.on('body:reshape', data => {
      this._viewer.reshapeBody(data)
    })
  }

  connected() {
    return this._connected
  }

  connect() {
    return new Promise((resolve, reject) => {
      let resolved = false
      // this._io = io()
      this._socket = this._io.socket('/')
      this._socket.on('connect', () => {
        console.log('connected!. socket', this._socket)
        this._connected = true
        this._wio.setSocket(this._socket)
        this._wio.setConnected(true)
        if (!resolved)
          resolve()
        resolved = true
        this._viewer.ioConnected()
        while( this._waiters.length) {
          const w = this._waiters.shift()
          w.resolve()
        }
      })

      this._socket.on('disconnect', () => {
        if (!resolved)
          reject()
        this._connected = false
        resolved = true
        this._viewer.ioDisconnected()
        this._wio.setConnected(false)
        while( this._waiters.length) {
          const w = this._waiters.shift()
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
    // this._viewer.prepareTest(this._test)
  }

  async resetTest(values) {
    this._test.reset(values)
    const resetResult = await this._socket.emitWithAck('world:reset')
    if (!resetResult || !resetResult.success) {
      console.warn("reset problems?", resetResult)
    }
    console.log('resetResult', resetResult)
    this._test.once('load:end', () => {
      const info = this._test.constructor.info()
      if (info.stages.configure)
        return this._viewer.configureTest()
      return this._viewer.runTest()
    })
    this._test.load(this._wio)
  }

  async testInputsChanged(fcode, values) {
    this._test.configure(this._wio, fcode, values)
  }

  async stepRunTest() {
    await this._wio.stepRunTest()
  }

  async toggleRunTest() {
    const res = await this._wio.toggleRunTest()
    if (!res || !res.success) {
      console.warn('core::toggleRun', res)
      return 'stop'
    }
    return res.status
  }

}
