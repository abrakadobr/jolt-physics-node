import Core from 'core'
import Viewer from 'viewer'
import { el, mount } from 'redom'

// const header = el('h5', 'Jolt physics – test 01')
// mount(document.getElementById('header'), header)

const viewer = new Viewer()
viewer.boot( document.getElementById('3d-view'))

const core = new Core()
core.boot(viewer)

core.connect().catch(err => console.error('connect failed', err))
