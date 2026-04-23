import Core from 'core'
import Viewer from 'viewer'


import { el, mount, text, setAttr } from 'redom'

const header = el('h5', 'Jolt physics library on nodejs backend')
mount(document.getElementById('header'), header)

const viewer = new Viewer()
viewer.boot(
  document.getElementById('3d-view'),
  document.getElementById('sidebar'),
  document.getElementById('buttons')
);

const core = new Core()
core.boot(viewer)

core.connect().then(res => {
  console.log('connect res', res)
}).catch(err => {
  console.error(err)
})
