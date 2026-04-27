// const EventEmitter = require('events')
// const Log = require('./log.js')
const LEE = require('./logee.js')

// const log = Log.m('Body')

class Body extends LEE {

  constructor(e, bodyInterface) {
    super()
    this.setLogName('Body')
    this._bi = bodyInterface
    this._sourceEvent = e
    this._bodyId = e.bodyId
    const params = e.params
    this._type = params.type
    this._subType = params.subType
    this._shape = params.shape
    this._position = params.position
    this._rotation = params.rotation
    this._layer = params.layer
    this._active = params.activate
    this._added = params.addToPhysics
    this._motionType = params.motionType
    this._transform = params.transform || null
  }

  snap() {
    return {
      id: this._bodyId,
      type: this._type,
      subType: this._subType,
      shape: this._shape,
      position: this._position,
      rotation: this._rotation,
      layer: this._layer,
      active: this._active,
      added: this._added,
      motionType: this._motionType,
      transform: this._transform
    }
  }

  id() { return this._bodyId }
  active() { return this._active }
  added() { return this._added }
  position() { return this._position }
  rotation() { return this._rotation }
  transform(next = null) {
    if (next) this._transform = next
    return this._transform
  }

  setAdded(next) {
    if (this._added === next) return
    this._added = next
    const e = next ? 'added' : 'removed'
    if (this._active && !next) this.setActive(false)
    this.emit(e, next) 
  }
  setActive(next) {
    if (this._active === next) return
    this._active = next
    const e = next ? 'activated' : 'deactivated'
    this.emit(e, next)
  }
  // auto async bcuz return async
  add(activate = false) { return this._bi.addBodyToEngine(this._bodyId, activate) }
  remove() { return this._bi.removeBodyFromEngine(this._bodyId) }
  activate() { return this._bi.activateBody(this._bodyId) }
  deactivate() { return this._bi.deactivateBody(this._bodyId) }
  destroy() { return this._bi.destroyBody(this._bodyId) }
}

module.exports = Body
