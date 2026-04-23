const EventEmitter = require('events')
const { createServer } = require("http")
const { Server } = require("socket.io")
const express = require('express')

class JoltServer extends EventEmitter {

  constructor(world) {
    super()
    this._world = world
    this._bi = this._world.bodyInterface()
    this._log = world.constructor.log('JServer')
    this._config = {
      host: '0.0.0.0',
      port: 8080,
      public: null,
      index: null
    }
    this._app = null
    this._server = null
    this._io = null
    this._world = null
    this._connections = 0
  }

  start(config = {}) {
    const cfg = { ...this._config, ...config }
    this._app = express()
    this._server = createServer(this._app)
    this._io = new Server(this._server, {
      transports: ['websocket']
    })
    this._io.on('connection', socket => {
      this.bindConnection(socket)
    })
    if (cfg.public && cfg.index) {
      this._app.use(express.static(cfg.public))
      this._app.get('/', (req, res) => {
        res.sendFile(`${cfg.public}/${cfg.index}`, { root: __dirname })
      })
    }
    this._bi.on('proxy', e => {
      io.emit('bi', e)
    })
    return new Promise(resolve => {
      this._server.listen(cfg.port, cfg.host, () => {
        this._log.info('jolt socket.io server listen on', `${cfg.host}:${cfg.port}`)
        resolve()
      })
    })
  }

  bindConnection(socket) {
    this._connections++
    this._log.info('new connection')
    const snap = this._bi.snap()
    socket.emit('snap', snap)
    socket.on('disconnect', (reason) => {
      this._log.info('socket disconnected')
      this._connections--
    })
  }
}

module.exports = JoltServer
