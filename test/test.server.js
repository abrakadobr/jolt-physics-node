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


io.on('connection', (socket) => {
  console.log('a user connected');

  socket.on('world:reset', async (cb) => {
    if (typeof cb !== 'function') return
    console.log('io:world:reset')
    world.reset()
    cb({success: true})
  })
});

server.listen(3000, () => {
  console.log('listening on *:3000');
});
