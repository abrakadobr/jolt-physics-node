import { el, mount, text, place, setChildren, setAttr } from 'redom'
import EE from './ee.js'
import * as THREE from 'three'

export default class Viewer extends EE {
  constructor() {
  super()
    this._viewerWrapper = null
    this._sidebarWrapper = null
    this._buttonsWrapper = null

    this._core = null

    this._camera = null
    this._renderer = null
    this._scene = null
    this._lastTime = 0

    this._sceneObjects = {}

    this._buttons = {}
    this._sidebar = {}

    this._state = {
      status: 'stop',
      test: ''
    }
    this._tests = {}
    this._connected = false
    this._test = null
    this._testInputs = {}
    this._testWidgets = {}
    this._testButtons = {}
  }

  bootButtons() {
    this._buttons.status = text('Offline')
    this._buttons.signal = el('div.badge.text-bg-secondary', this._buttons.status )
    this._buttons.toggle = el('button.btn.btn-sm.btn-secondary.mx-1', this._state.status === 'run' ? 'stop' : 'run' )
    this._buttons.step = el('button.btn.btn-sm.btn-secondary.mx-1', this._state.status === 'step' ? 'next step' : 'run by step' )
    this._buttons.wrapper = el('div.flex-fill.d-flex.flex-row.justify-content-end.align-items-center', [this._buttons.step, this._buttons.toggle, this._buttons.signal])
    mount(this._buttonsWrapper, this._buttons.wrapper)

    setAttr(this._buttons.toggle, { disabled: 1 })
    setAttr(this._buttons.step, { disabled: 1 })
  }

  bootSidebar() {
    this._sidebar.options = {
      empty: el('option', { value: '' }, ''),
      dropABall: el('option', { value: 'dropABall' }, 'Drop a ball')
    }
    this._sidebar.testSelector = el('select.form-select.form-select-sm', { placeholder: 'Select test'}, [
      this._sidebar.options.empty,
      this._sidebar.options.dropABall
    ])
    this._sidebar.testSelector.addEventListener('change', () => {
      this.testSelectorChanged( this._sidebar.testSelector.value)
    })
    // console.log('select', this._sidebar.testSelector)
    this._sidebar.scrollWindow = el('div.flex-fill.scroll-region.d-flex.flex-column')
    this._sidebar.wrapper = el('div.d-flex.flex-fill.flex-column.justify-content-top.align-items-stretch', [
      this._sidebar.testSelector, this._sidebar.scrollWindow
    ])
    mount(this._sidebarWrapper, this._sidebar.wrapper)
  }

  boot(viewerWrapper, sidebarWrapper, buttonsWrapper) {
    this._viewerWrapper = viewerWrapper
    this._sidebarWrapper = sidebarWrapper
    this._buttonsWrapper = buttonsWrapper

    this.bootButtons()
    this.bootSidebar()
    const rect = this._viewerWrapper.getBoundingClientRect()
    this._scene = new THREE.Scene();
    this._camera = new THREE.PerspectiveCamera( 75, rect.width / rect.height, 0.1, 1000 );
    this._renderer = new THREE.WebGLRenderer();
    this._canvas = this._renderer.domElement;
    mount(this._viewerWrapper, this._canvas)
    console.log({viewerWrapper, sidebarWrapper})
    this.onResize()

    window.addEventListener('resize', () => {
      this.onResize()
    })
    this._renderer.setAnimationLoop((time) => {
      this.animate(time)
    })
    this.dropEmptyPlaceholder()
  }

  onResize() {
    if (!this._viewerWrapper) return
    const rect = this._viewerWrapper.getBoundingClientRect()
    this._camera.aspect = rect.width / rect.height;
    this._camera.zoom = this._camera.aspect<=1?this._camera.aspect:1;
    this._camera.updateProjectionMatrix();
    this._renderer.setSize( rect.width, rect.height )
  }

  animate(time) {
    if (!this._lastTime) {
      this._lastTime = time
      return
    }
    const delta = time - this._lastTime
    this._lastTime = time
    if (this._sceneObjects.cube) {
      this._sceneObjects.cube.rotation.x = time/2000
      this._sceneObjects.cube.rotation.z = time/1000
    }
    this._renderer.render( this._scene, this._camera );
  }

  core(cr) {
    this._core = cr
  }

  dropEmptyPlaceholder() {
    const geometry = new THREE.BoxGeometry( 1, 1, 1 );
    const material = new THREE.MeshBasicMaterial( { color: 0x00ff00 } );
    const cube = new THREE.Mesh( geometry, material );
    this._scene.add( cube );

    this._camera.position.z = 5;
    this._sceneObjects.cube = cube
  }

  ioConnected() {
    this._buttons.status.textContent = 'Online'
    setAttr(this._buttons.signal, {
      class: 'badge text-bg-success'
    })
    setAttr(this._sidebar.testSelector, { disabled: 0 })
  }

  ioDisconnected() {
    this._buttons.status.textContent = 'Offline'
    setAttr(this._buttons.signal, {
      class: 'badge text-bg-secondary'
    })
    setAttr(this._sidebar.testSelector, { disabled: 1 })
  }

  testSelectorChanged(next) {
    if (next === this._state.test) return
    console.log('next test', next)
    this._core.testSelected(next, this._sidebar.scrollWindow)
  }

  setTests(tests) {
    this._tests = tests
    setChildren(this._sidebar.testSelector, [])
    const options = Object.keys(this._tests).map(code => {
      const params = {}
      if (code === this._state.test)
        params.selected = true
      return el('option', { ...params, value: code }, this._tests[code].name)
    })
    options.unshift(el('option', { value: "" }, ''))
    setChildren(this._sidebar.testSelector, options)
    console.log('tests setted', this._tests)
  }

  setTest(test) {
    this._test = test
    this._test.on('load:start', this, this.prepareTest)
  }

  prepareTest() {
    // console.log(this._sidebar, this._test)
    // this._sidebar.scrollWindow.update(false)
    this._testInputs = {}
    this._testWidgets = {}
    const ui = []
    const info = this._test.constructor.info()
    // console.log('prepare test', info)
    Object.keys(info.inputs).forEach(fcode => {
      const field = info.inputs[fcode]
      if (field.type === 'Vec3') {
        this._testInputs[fcode] = {x: 0, y: 0, z: 0 }
        const vecX = el('input.form-control', { type: 'number', step: 0.1, value: 0, placeholder: 'X' })
        const vecY = el('input.form-control', { type: 'number', step: 0.1, value: 0, placeholder: 'Y' })
        const vecZ = el('input.form-control', { type: 'number', step: 0.1, value: 0, placeholder: 'Z' })
        vecX.addEventListener('change', () => {
          this._testInputs[fcode].x = vecX.value
        })
        vecY.addEventListener('change', () => {
          this._testInputs[fcode].y = vecY.value
        })
        vecZ.addEventListener('change', () => {
          this._testInputs[fcode].z = vecZ.value
        })
        const f = el('div', [el('label.pb-1.pt-2', [text(field.name)]), el('div.d-flex.flex-row.align-items-center', [
          el('span.pe-1','X'), vecX, el('span.px-1', 'Y'), vecY, el('span.px-1', 'Z'), vecZ
        ])])
        this._testWidgets[fcode] = { x: vecX, y: vecY, z: vecZ }
        setAttr(this._testWidgets[fcode].x, {disabled: 1})
        setAttr(this._testWidgets[fcode].y, {disabled: 1})
        setAttr(this._testWidgets[fcode].z, {disabled: 1})
        ui.push(f)
      }
      if (field.type === 'float') {
        this._testInputs[fcode] = 0
        const finput = el('input.form-control', { type: 'number', step: 0.1, value: 0  })
        this._testWidgets[fcode] = finput
        setAttr(this._testWidgets[fcode], {disabled: 1})
        const f = el('div', [el('label.py-2', [text(field.name)]), el('div.d-flex.flex-row', [
          finput
        ])])
        ui.push(f)
      }
    })
    this._testButtons = {}
    this._testButtons.reset = el('button.btn.btn-sm.btn-outline-secondary.mt-2', 'Reset')
    this._testButtons.reset.addEventListener('click', () => {
      this.resetTest()
    })
    ui.unshift(this._testButtons.reset)
    setChildren(this._sidebar.scrollWindow, ui)
    this.resetTest()
  }

  resetTest() {
    const info = this._test.constructor.info()
    Object.keys(info.inputs).forEach(fcode => {
      const field = info.inputs[fcode]
      if (field.type === 'Vec3') {
        setAttr(this._testWidgets[fcode].x, { value: field.default.x })
        setAttr(this._testWidgets[fcode].y, { value: field.default.y })
        setAttr(this._testWidgets[fcode].z, { value: field.default.z })
        this._testInputs[fcode].x = field.default.x
        this._testInputs[fcode].y = field.default.y
        this._testInputs[fcode].z = field.default.z
      }
      if (field.type === 'float') {
        setAttr(this._testWidgets[fcode], { value: field.default })
        this._testInputs[fcode] = field.default
      }
    })
    this._core.resetTest()
  }
}
