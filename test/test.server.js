const express = require('express');
const app = express();
const http = require('http');
const server = http.createServer(app);
const { Server } = require("socket.io");
const io = new Server(server);
const path = require('path')
const Jolt = require('../jolt.js')

app.use(express.static(path.join(__dirname, 'webview')));

app.get('/', (req, res) => {
  res.sendFile(__dirname + '/webflow/index.html');
});


const world = new Jolt.World();
// console.log('world', world, world.initialize)
world.initialize({
  memoryPreallocatedMb: 10,
  maxBodies: 10000,
  maxBodyMutexes: 0,
  maxBodiesPairs: 10000,
  gravity: 9.8
})

const lm = world.layersManager()
const bm = world.bodiesManager()

world.on('body-created', e => {
  console.log('world@body-created', e)
  io.emit('body:created', {
    id: e.id(),
    shape: e.shape(),
    transform: e.transform(),
    comTransform: e.comTransform(),
    type: e.getType()
  })
})

world.on('body-activation', e => {
  console.log('world@body-activation', e)
  io.emit('body:activation', e)
})

world.on('body-reshape', e => {
  console.log('world@body-reshape', e)
  io.emit('body:reshape', {
    id: e.id(),
    shape: e.shape(),
    transform: e.transform(),
    comTransform: e.comTransform(),
    type: e.getType()
  })
})

io.on('connection', (socket) => {
  console.log('a user connected');

  socket.on('world:reset', async (cb) => {
    if (typeof cb !== 'function') return
    console.log('io:world:reset')
    world.reset()
    cb({success: true})
  })

  socket.on('layers:get', cb => {
    if (typeof cb !== 'function') return
    const ids = lm.layersIDs()
    const ret = ids.map(lid => {
      const name = lm.layerName(lid)
      return [lid, name]
    })
    cb(ret)
  })

  socket.on('body>create', ({ type, shape, settings }, cb) => {
    if (typeof cb !== 'function') return
    let fn = 'createEmpty'
    if (type === 'box') fn = 'createBox'
    if (type === 'sphere') fn = 'createSphere'
    console.log('body:create', { type, shape, settings, fn})
    if (fn === 'createEmpty') return cb(null)
    try {
      const body = bm[fn](shape, settings)
      //console.log('body:create', body, settings)
      if (!body || typeof body.id !== 'function') return cb(null)
      body.on('transform', event => {
        console.log('body@transform', event, )
        const transform = body.transform()
        socket.emit('body:transform', { ...event, transform })
      })
      /*
      body.on('body-activation', event => {
        console.log('body@activation', event, )
        const transform = body.transform()
        socket.emit('body:activation', { ...event, transform })
      })
      */
      const transform = body.transform()
      cb({ id: body.id(), type: body.getType(), transform, shape })
    } catch (err) {
      console.error('error', err)
      return cb(null)
    }
  })

  socket.on('bodies:delete', async (list, cb) => {
    if (typeof cb !== 'function') return
    bm.removeBodiesI(list)
    cb({ success: true })
  })

  socket.on('body:set:position', async ( {bodyId, position}, cb) => {
    if (typeof cb !== 'function') return
    if (!bodyId) return cb({success: false, error: 'BODY_ID_EMPTY'})
    bm.setBodyPositionI(bodyId, position)
    cb({success: true})
  })

  socket.on('body:reshape', async ({ bodyId, params }, cb) => {
    if (typeof cb !== 'function') return
    if (!bodyId) return cb({success: false, error: 'BODY_ID_EMPTY'})
    let fn = ''
    if (params.type === 'sphere') fn = 'reshapeBodyToSphereI'
    if (!fn) return cb({success: false, error: 'SHAPE_TYPE_ERROR'})
    bm[fn](bodyId, params.shape)
    cb({success: true})
  })

  socket.on('run:step', (cb) => {
    if (typeof cb !== 'function') return
    world.stepPhysics()
    cb({success: true})
  })

  socket.on('run:toggle', (cb) => {
    if (typeof cb !== 'function') return
    const state = world.state()
    if (state === 'stop') {
      world.worldRun(1.0, true)
    } else {
      world.worldStop()
    }
    const status = world.state()
    console.log('toogle status', status)
    cb({success: true, status})
  })
})

server.listen(3000, () => {
  console.log('listening on *:3000');
  //world.worldRun(1.0, true)
});
