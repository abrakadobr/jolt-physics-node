export default class EE {
  constructor() {
    this._ee_subscribers = {}
    this._ee_subscribers_once = {}
  }

  on(event, obj, cb) {
    // console.log('EE::on', this, event, typeof obj, typeof cb, !!cb)
    if (!this._ee_subscribers[event])
      this._ee_subscribers[event] = []
    if (typeof obj === 'function' && !cb)
      this._ee_subscribers[event].push(obj)
    if (typeof obj === 'object' && typeof cb === 'function')
      this._ee_subscribers[event].push({obj, cb})
    return this
  }

  once(event, obj, cb) {
    if (!this._ee_subscribers_once[event])
      this._ee_subscribers_once[event] = []
    if (typeof obj === 'function' && !cb)
      this._ee_subscribers_once[event].push(obj)
    if (typeof obj === 'object' && typeof cb === 'function')
      this._ee_subscribers_once[event].push({obj, cb})
    return this
  }

  off(event, obj, cb) {
    // console.log('EE::off', this, event)
    if (!event && !obj && !cb) {
      this._ee_subscribers = {}
      this._ee_subscribers_once = {}
      return
    }
    if (!event) return
    if (!this._ee_subscribers[event] && !this._ee_subscribers_once[event]) return
    if (!obj && !cb) {
      this._ee_subscribers[event] = []
      this._ee_subscribers_once[event] = []
      return
    }
    this._ee_subscribers[event] = (this._ee_subscribers[event] || []).filter(el => {
      if (typeof obj === 'function' && !cb) {
        return !(typeof el === 'function' && el === obj)
      }
      if (typeof obj === 'obj' && typeof cb === 'function' && typeof el === 'object' && typeof el.obj === 'object' && typeof el.cb === 'function')
        return !(el.obj === obj && el.cb === cb )
      return true
    })
    this._ee_subscribers_once[event] = (this._ee_subscribers_once[event] || []).filter(el => {
      if (typeof obj === 'function' && !cb) {
        return !(typeof el === 'function' && el === obj)
      }
      if (typeof obj === 'obj' && typeof cb === 'function' && typeof el === 'object' && typeof el.obj === 'object' && typeof el.cb === 'function')
        return !(el.obj === obj && el.cb === cb )
      return true
    })
  }

  emit(event, data) {
    // console.log('EE::emit', this, event, this._ee_subscribers[event])
    if (this._ee_subscribers_once[event]) {
      for (let i = 0; i < this._ee_subscribers_once[event].length; i++) {
        const callback = this._ee_subscribers_once[event][i];
        if (typeof callback === 'function')
          callback(data);
        if (typeof callback === 'object' && typeof callback.obj === 'object' && typeof callback.cb === 'function')
          callback.cb.call(callback.obj, data)
      }
      this._ee_subscribers_once[event] = []
    }

    if (!this._ee_subscribers[event]) return

    for (let i = 0; i < this._ee_subscribers[event].length; i++) {
      const callback = this._ee_subscribers[event][i];
      if (typeof callback === 'function')
        callback(data);
      if (typeof callback === 'object' && typeof callback.obj === 'object' && typeof callback.cb === 'function')
        callback.cb.call(callback.obj, data)
    }
  }
}
