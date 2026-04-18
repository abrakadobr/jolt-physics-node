const Log = require('./log.js')
const EventEmitter = require('events')

class LoggedEventEmitter extends EventEmitter {

  constructor() {
    super()
    this._logName = this.constructor.name
    this._log = Log.m(this._logName)
  }

  setLogName(next) {
    this._logName = this.constructor.name
    this._log = Log.m(this._logName)
  }

  L() {
    return this._log
  }

}

module.exports = LoggedEventEmitter
