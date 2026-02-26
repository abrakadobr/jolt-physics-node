'use strict';
/**
 * Physics worker script. Runs a World in a Worker Thread.
 * Communicates with PhysicsWorker via postMessage.
 *
 * Message protocol:
 *   → { id, cmd, args }   (from main thread)
 *   ← { id, result }      (response for commands that return values)
 *   ← { id, error }       (on error)
 *   ← { type: 'event', kind, data }  (async events: bodyActivation, contact)
 *   ← { type: 'state', snapshot }    (after each step, if autoState is true)
 */

const { parentPort, workerData } = require('worker_threads');
const { World } = require(workerData.indexPath);

let world = null;
let autoState = workerData.autoState !== false;

function reply(id, result) {
  parentPort.postMessage({ id, result });
}

function replyError(id, message) {
  parentPort.postMessage({ id, error: message });
}

function handle(id, cmd, args) {
  switch (cmd) {
    case 'init': {
      if (world) world.destroy();
      world = new World(args[0] || {});
      if (args[1] != null) autoState = Boolean(args[1]);
      // wire up events
      world.onBodyActivation((ev) => parentPort.postMessage({ type: 'event', kind: 'bodyActivation', data: ev }));
      world.onContact((ev) => parentPort.postMessage({ type: 'event', kind: 'contact', data: ev }));
      reply(id, true);
      break;
    }

    case 'destroy': {
      if (world) { world.destroy(); world = null; }
      reply(id, true);
      break;
    }

    case 'step': {
      const dt = args[0];
      world.step(dt);
      if (autoState) {
        const snapshot = world.snapshotState();
        parentPort.postMessage({ type: 'state', snapshot }, [snapshot.buffer]);
      }
      reply(id, true);
      break;
    }

    case 'snapshotState': {
      const snapshot = world.snapshotState();
      // Transfer the underlying ArrayBuffer to avoid copy
      reply(id, snapshot);
      break;
    }

    case 'applySnapshot': {
      reply(id, world.applySnapshot(args[0]));
      break;
    }

    case 'saveScene': {
      reply(id, world.saveScene());
      break;
    }

    case 'loadScene': {
      reply(id, world.loadScene(args[0]));
      break;
    }

    default: {
      // Generic passthrough: world[cmd](...args)
      if (typeof world[cmd] !== 'function') {
        replyError(id, `Unknown command: ${cmd}`);
        return;
      }
      try {
        const result = world[cmd](...args);
        reply(id, result === undefined ? null : result);
      } catch (e) {
        replyError(id, e.message || String(e));
      }
    }
  }
}

parentPort.on('message', ({ id, cmd, args }) => {
  try {
    handle(id, cmd, args || []);
  } catch (e) {
    replyError(id, e.message || String(e));
  }
});
