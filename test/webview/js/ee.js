export default class EE {
  constructor() {
    this._ee_subscribers = {}
  }

  on(event, obj, cb) {
    if (!this._ee_subscribers[event])
      this._ee_subscribers[event] = []
    if (typeof obj === 'function' && !cb)
      this._ee_subscribers[event].push(cb)
    if (typeof obj === 'object' && typeof cb === 'function')
      this._ee_subscribers[event].push({obj, cb})
    return this
  }

  off(event, obj, cb) {
    if (!event && !obj && !cb) {
      this._ee_subscribers = {}
      return
    }
    if (!event) return
    if (!this._ee_subscribers[event]) return
    if (!obj && !cb) {
      this._ee_subscribers[event] = []
      return
    }
    this._ee_subscribers[event] = this._ee_subscribers[event].filter(el => {
      if (typeof obj === 'function' && !cb) {
        return !(typeof el === 'function' && el === obj)
      }
      if (typeof obj === 'obj' && typeof cb === 'function' && typeof el === 'object' && typeof el.obj === 'object' && typeof el.cb === 'function')
        return !(el.obj === obj && el.cb === cb )
      return true
    })
  }

  emit(event, data) {
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
