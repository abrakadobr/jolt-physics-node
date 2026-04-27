import EE from './ee.js'

export class WorldIO extends EE {
  constructor(core, socket) {
    super()
    this._core = core
    this._socket = null
    this._connected = false
    this._layers = {}
    this._bodies = {}
  }

  setSocket(next) {
    this._socket = next
    if (!this._socket) return
    this._socket.on('snap', snap => {
      if (!snap || typeof snap !== 'object') return
      Object.values(snap).forEach(body => {
        if (!body || this._bodies[body.id]) return
        this._bodies[body.id] = body
        this.emit('body:created', body)
      })
    })
    this._socket.on('bi', e => {
      if (!e || !e.type) return
      if (e.type === 'bodyCreated' && e.success) {
        const p = e.params
        const body = {
          id: e.bodyId,
          ...p
        }
        this._bodies[body.id] = body
        this.emit('body:created', body)
      } else if (e.type === 'bodyTransform') {
        this.emit('body:transform', { body: e.bodyId, transform: e.transform })
      } else if (e.type === 'bodyDestroyed') {
        delete this._bodies[e.bodyId]
      }
    })
    this._socket.on('snap', snap => {
      console.log('SNAP', snap)
      Object.keys(snap).forEach(bid => {
        if (!this._bodies[bid]) {
          this._bodies[bid] = snap[bid]
          this.emit('body:created', this._bodies[bid])
        }
      })
    })
  }

  setConnected(next) {
    this._connected = next
  }

  async defineStandartLayers() {
    const layers = await this._socket.emitWithAck('layers:get')
    const toCreate = []
    if (!layers || !layers.length) {
      toCreate.push('static')
      toCreate.push('dynamic')
    } else {
      if (!layers.find(l => l.includes('static')))
        toCreate.push('static')
      if (!layers.find(l => l.includes('dynamic')))
        toCreate.push('dynamic')
    }
    if (!toCreate.length) return
    console.log('DSL: ', layers, 'TO CREATE', toCreate)
  }

  async createBody(params) {
    const body = await this._socket.emitWithAck('body>create', params)
    if (!body) return body
    //this._bodies[body.id] = body
    //console.log('>body:create', body)
    //this.emit('body:create', body)
    return body.id
  }

  async deleteBodies(listIds) {
    const result = await this._socket.emitWithAck('bodies:delete', listIds)
    if (result.success) listIds.forEach(bid => {
      if (!this._bodies[bid]) return
      delete this._bodies[bid]
      this._bodies[bid] = null
    })
  }

  async setBodyPosition(bodyId, position) {
    const result = await this._socket.emitWithAck('body:set:position', { bodyId, position })
    console.log('wio<body:set:position', bodyId, position, result)
  }

  async reshapeBody(bodyId, params) {
    const result = await this._socket.emitWithAck('body:reshape', { bodyId, params })
    console.log('wio<body:reshape', bodyId, params, result)
  }

  async stepRunTest() {
    const result = await this._socket.emitWithAck('run:step')
    console.log('wio<stepRun', result)
  }

  async toggleRunTest() {
    const result = await this._socket.emitWithAck('run:toggle')
    console.log('wio<stepRun', result)
    return result
  }
}
