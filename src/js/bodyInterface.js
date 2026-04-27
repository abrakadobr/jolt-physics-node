// const EventEmitter = require('events')
const Body = require('./body.js')
// const Log = require('./log.js')
const LEE = require('./logee.js')

// const log = Log.m('BI')

class BodyInterface extends LEE {

  constructor(world) {
    super()
    this.setLogName('BI')
    this._world = world;
    this._requests = {}
    this._bodies = {}
  }

  snap() {
    return Object.keys(this._bodies).reduce((acc, bid) => {
      acc[bid] = this._bodies[bid].snap()
      return acc
    }, {})
  }

  createBox(halfExtend, position, rotation, motionType, layer, addToPhysics = true, activate = false) {
    return new Promise(resolve => {
      const cid = this._world.exec({
        cmd: 'createBody',
        params: {
          type: 'convex',
          subType: 'box',
          shape: { halfExtend },
          position,
          rotation,
          motionType,
          layer,
          addToPhysics,
          activate
        }
      })
      this._requests[cid] = resolve
    })
  }

  createSphere(radius, position, rotation, motionType, layer, addToPhysics = true, activate = false) {
    return new Promise(resolve => {
      const cid = this._world.exec({
        cmd: 'createBody',
        params: {
          type: 'convex',
          subType: 'sphere',
          shape: { radius },
          position,
          rotation,
          motionType,
          layer,
          addToPhysics,
          activate
        }
      })
      this._requests[cid] = resolve
    })
  }

  addBodyToEngine(bid, activate = false) {
    return new Promise(resolve => {
      const body = this._bodies[bid]
      // this.L().warn('body?', [bid, body])
      if (!body) return resolve(false)
      if (body.added()) return resolve(true)
      const cid = this._world.exec({
        cmd: 'addBody',
        bodyId: bid,
        activate
      })
      this._requests[cid] = resolve
    })
  }

  removeBodyFromEngine(bid) {
    return new Promise(resolve => {
      const body = this._bodies[bid]
      if (!body) return resolve(false)
      if (!body.added()) return resolve(true)
      const cid = this._world.exec({
        cmd: 'removeBody',
        bodyId: bid
      })
      this._requests[cid] = resolve
    })
  }

  activateBody(bid) {
    return new Promise(resolve => {
      const body = this._bodies[bid]
      if (!body) return resolve(false)
      if (!body.added() || body.active()) return resolve(true)
      const cid = this._world.exec({
        cmd: 'activateBody',
        bodyId: bid
      })
      this._requests[cid] = resolve
    })
  }

  deactivateBody(bid) {
    return new Promise(resolve => {
      const body = this._bodies[bid]
      if (!body) return resolve(false)
      if (!body.active()) return resolve(true)
      const cid = this._world.exec({
        cmd: 'deactivateeBody',
        bodyId: bid
      })
      this._requests[cid] = resolve
    })
  }

  destroyBody(bid) {
    return new Promise(resolve => {
      const body = this._bodies[bid]
      if (!body) return resolve(false)
      const cid = this._world.exec({
        cmd: 'destroyBody',
        bodyId: bid
      })
      this._requests[cid] = resolve
    })
  }

  transformBody(e) {
    if (!e.bodyId || !this._bodies[e.bodyId]) {
      this.L().warn('@bodyTransform no body', e)
      return
    }
    this._bodies[e.bodyId].transform(e.transform)
    this.emit('proxy', e)
  }

  processEvent(e) {
    const cid = e.commandId
    this.L().warn('process event',[ e.type, e.success])
    if (e.type === 'bodyCreated') {
      if (!e.success) {
        if (this._requests[cid]) {
          this._requests[cid](null)
          delete this._requests[cid]
        }
        return
      }
      const body = new Body(e, this)
      this.L().info('body created', [body.id()])
      this._bodies[body.id()] = body
      if (this._requests[cid]) {
        this._requests[cid](body)
        delete this._requests[cid]
      }
      this.emit('bodyCreated', body)
      this.emit('proxy', e)
      return
    }
    if (e.type === 'bodyAdded') {
      const cid = e.commandId
      const resolve = this._requests[cid]
      const bid = e.bodyId
      this.L().info('body added!!', [bid, e.success])
      if (e.success && !this._bodies[bid]) {
        this.L().warn('unknown body event @added', bid)
        return resolve(false)
      }
      this._bodies[bid].setAdded(e.success)
      if (resolve) {
        resolve(e.success)
        delete this._requests[cid]
      }
      this.emit('proxy', e)
      return
    }
    if (e.type === 'bodyRemoved') {
      const cid = e.commandId
      const resolve = this._requests[cid]
      const bid = e.bodyId
      if (e.success && !this._bodies[bid]) {
        this.L().warn('unknown body event @removed', bid)
        return resolve(false)
      }
      if (e.success)
        this._bodies[bid].setAdded(false)
      if (resolve) {
        resolve(e.success)
        delete this._requests[cid]
      }
      this.emit('proxy', e)
      return
    }
    if (e.type === 'bodyActivated') {
      const cid = e.commandId
      const resolve = this._requests[cid]
      const bid = e.bodyId
      if (e.success && !this._bodies[bid]) {
        this.L().warn('unknown body event @activated', bid)
        if (typeof resolve === 'function')
          return resolve(false)
        return
      }
      this._bodies[bid].setActive(e.success)
      if (resolve) {
        resolve(e.success)
        delete this._requests[cid]
      }
      this.emit('proxy', e)
      return
    }
    if (e.type === 'bodyDeactivated') {
      const cid = e.commandId
      const resolve = this._requests[cid]
      const bid = e.bodyId
      if (e.success && !this._bodies[bid]) {
        this.L().warn('unknown body event @deactivated', bid)
        return resolve(false)
      }
      if (e.success)
        this._bodies[bid].setActive(false)
      if (resolve) {
        resolve(e.success)
        delete this._requests[cid]
      }
      this.emit('proxy', e)
      return
    }
    if (e.type === 'bodyDestroyed') {
      const cid = e.commandId
      const resolve = this._requests[cid]
      const bid = e.bodyId
      if (e.success && !this._bodies[bid]) {
        this.L().warn('unknown body event @destroyed', bid)
        return resolve()
      }
      if (e.success) {
        this._bodies[bid].emit('destroyed')
        setTimeout(() => {
          delete this._bodies[bid]
        },0)
      }
      if (resolve) {
        resolve()
        delete this._requests[cid]
      }
      this.emit('proxy', e)
      return
    }
  }



}

module.exports = BodyInterface
