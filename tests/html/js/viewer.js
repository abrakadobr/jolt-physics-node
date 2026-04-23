import { el, mount, text, place, setChildren, setAttr } from 'redom'
import EE from './ee.js'
import * as THREE from 'three'
// import { OrbitControls } from 'three'
// console.log(THREE)
import { OrbitControls } from '/js/OrbitControls.js';
console.log('OrbitControls', OrbitControls)

const clearThree = (obj) => {
  if (!obj) return
  if (Array.isArray(obj.children)) {
    while(obj.children.length > 0){ 
      clearThree(obj.children[0])
      obj.remove(obj.children[0])
    }
  }
  if(obj.geometry) obj.geometry.dispose()

  if(obj.material){ 
    //in case of map, bumpMap, normalMap, envMap ...
    Object.keys(obj.material).forEach(prop => {
      if(!obj.material[prop])
        return
      if(obj.material[prop] !== null && typeof obj.material[prop].dispose === 'function')
        obj.material[prop].dispose()
    })
    obj.material.dispose()
  }
}   

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
      test: '',
      stage: 'none'
    }
    this._tests = {}
    this._connected = false
    this._test = null
    this._testInputs = {}
    this._testWidgets = {}
    this._testButtons = {}
    this._lights = {}
  }

  bootButtons() {
    this._buttons.status = text('Offline')
    this._buttons.signal = el('div.badge.text-bg-secondary', this._buttons.status )
    this._buttons.wrapper = el('div.flex-fill.d-flex.flex-row.justify-content-end.align-items-center', [this._buttons.signal])
    mount(this._buttonsWrapper, this._buttons.wrapper)

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
    this._renderer.setClearColor(0x454545);
    this._renderer.shadowMap.enabled = true
    this._renderer.shadowMap.type = THREE.PCFSoftShadowMap
    this._renderer.outputColorSpace = THREE.SRGBColorSpace;
    this._canvas = this._renderer.domElement;

    this._controls = new OrbitControls(this._camera, this._renderer.domElement);
    this._controls.enableDamping = true;
    mount(this._viewerWrapper, this._canvas)
    console.log({viewerWrapper, sidebarWrapper})
    this.onResize()

    window.addEventListener('resize', () => {
      this.onResize()
    })
    this._renderer.setAnimationLoop((time) => {
      this._controls.update();
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
    console.log('DEPO[')
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

  shapeGeometry(shape) {
    console.log('shape geom', shape)
    if (shape.type === 'convex') {
      if (shape.subType === 'box') {
        const s = shape.halfExtent
        return new THREE.BoxGeometry(s.x * 2, s.y * 2, s.z * 2)
      }
      if (shape.subType === 'sphere') {
        return new THREE.SphereGeometry(shape.radius, 16, 16)
      }
    }
  }

  ioBodyCreated(body) {
    console.log('viewer@body:created', body)
    const bid = `body${body.id}`
    if (this._sceneObjects[bid]) {
      console.error('viewer@body:created id already exists!', body, this._sceneObjects[bid])
      return this.ioBodyUpdate(body)
    }
    const mat = new THREE.MeshStandardMaterial({
      wireframe: true,
      color: new THREE.Color(0xFFFF00)
    })
    const geom = this.shapeGeometry(body.shape)
    console.log('geom?', geom)
    if (!geom) return
    const mesh = new THREE.Mesh(geom, mat)
    // const mtx = new THREE.Matrix4().fromArray(body.transform)
    const mtxcom = new THREE.Matrix4().fromArray(body.comTransform)
    // mesh.matrix.copy(mtx)
    // mesh.matrix.decompose(mesh.position, mesh.quaternion, mesh.scale)
    mesh.applyMatrix4(mtxcom)
    mesh.updateMatrixWorld(true)
    console.log(mesh.position, mesh.scale)
    this._sceneObjects[bid] = mesh
    this._scene.add(mesh)
  }

  ioBodyUpdate(body) {

  }

  testSelectorChanged(next) {
    if (next === this._state.test) return
    console.log('next test', next)
    this._core.testSelected(next, this._sidebar.scrollWindow)
  }

  setTests(tests) {
    this._tests = tests
    this._state.stage = 'none'
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

  prepareTest(test) {
    this._test = test
    this._state.stage = 'load'
    // this._test.on('load:start', this, this.prepareTest)
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
          this._testInputs[fcode].x = parseFloat(vecX.value)
          this.inputsChanged(fcode)
        })
        vecY.addEventListener('change', () => {
          this._testInputs[fcode].y = parseFloat(vecY.value)
          this.inputsChanged(fcode)
        })
        vecZ.addEventListener('change', () => {
          this._testInputs[fcode].z = parseFloat(vecZ.value)
          this.inputsChanged(fcode)
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
        finput.addEventListener('change', () => {
          this._testInputs[fcode] = parseFloat(finput.value)
          this.inputsChanged(fcode)
        })
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

    this._buttons.toggle = el('button.btn.btn-sm.btn-secondary.mx-1.flex-grow-1', this._state.status === 'run' ? 'stop' : 'run' )
    this._buttons.toggle.addEventListener('click', () => {
      this.toggleRunTest()
    })
    this._buttons.step = el('button.btn.btn-sm.btn-secondary.mx-1.flex-grow-1', this._state.status === 'step' ? 'next step' : 'run by step' )
    this._buttons.step.addEventListener('click', () => {
      this.stepRunTest()
    })
    setAttr(this._buttons.toggle, { disabled: 1 })
    setAttr(this._buttons.step, { disabled: 1 })
    this._buttons.controls = el('div.mt-2.d-flex.flex-row',[ this._buttons.toggle, this._buttons.step ])
    ui.push(this._buttons.controls)

    setChildren(this._sidebar.scrollWindow, ui)
    this.resetTest()
  }

  resetTest() {
    this._state.stage = 'load'
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
    this.resetView();
    this._core.resetTest(this._testInputs)
  }

  resetView() {
    clearThree(this._scene)
    this._lights = {}
    console.log('scene cleared')
    this._camera.position.x = 5
    this._camera.position.y = 5
    this._camera.position.z = 5
    this._camera.lookAt(0,0,0)
    const gridHelper = new THREE.GridHelper(20, 20, 0x707070, 0x555555 )
    this._scene.add(gridHelper)
    if (!this._lights) this._lights = {}
    if (!this._lights.ambient) {
      this._lights.ambient = new THREE.AmbientLight(0xFFFFFF, 0.3)
      this._scene.add(this._lights.ambient)
    }
    if (!this._lights.directional) {
      this._lights.directional = new THREE.DirectionalLight(0xFFFFFF, 1.0)
      this._lights.directional.position.set(5, 15, 5)
      this._lights.directional.target.position.set(0, 0, 0)
      this._lights.directional.castShadows = true
      this._lights.directional.shadow.mapSize.set(2048, 2048);

      this._lights.directional.shadow.camera.left = -10;
      this._lights.directional.shadow.camera.right = 10;
      this._lights.directional.shadow.camera.top = 10;
      this._lights.directional.shadow.camera.bottom = -10;

      this._lights.directional.shadow.camera.near = 0.5;
      this._lights.directional.shadow.camera.far = 50;
      this._scene.add(this._lights.directional)
    }

    /*
    if (!this._lights.hemi) {
      this._lights._hemi = new THREE.HemisphereLight(
        0xffffff, // небо
        0x444444, // земля
        0.5
      );
      this._scene.add(this._lights.hemi);
    } 
    */
    console.log('view resetd') 
  }

  configureTest() {
    this._state.stage = 'configure'
    const info = this._test.constructor.info()
    if (!info.stages.configure) return this.toggleRunTest()
    Object.keys(info.inputs).forEach(fcode => {
      const field = info.inputs[fcode]
      const disabled = !field.stages.includes('configure')
      if (field.type === 'Vec3') {
        setAttr(this._testWidgets[fcode].x, { disabled: disabled ? 1 : 0 })
        setAttr(this._testWidgets[fcode].y, { disabled: disabled ? 1 : 0 })
        setAttr(this._testWidgets[fcode].z, { disabled: disabled ? 1 : 0 })
      }
      if (field.type === 'float') {
        setAttr(this._testWidgets[fcode], { disabled: disabled ? 1 : 0 })
      }
    })
    setAttr(this._buttons.toggle, { disabled: 0 })
    setAttr(this._buttons.step, { disabled: 0 })
  }

  inputsChanged(fcode) {
    this._core.testInputsChanged(fcode, this._testInputs)
  }

  setRunStatus(next) {
    console.log('viewer::run status', next)
    this._state.status = next
    if (this._state.status === 'stop') {
      setAttr(this._buttons.toggle, { disabled: 0 })
      setAttr(this._buttons.step, { disabled: 0 })
      this._buttons.toggle.textContent = 'Run'
      this._buttons.step.textContent = 'Switch to step'
    }
    if (this._state.status === 'run') {
      setAttr(this._buttons.toggle, { disabled: 0 })
      setAttr(this._buttons.step, { disabled: 0 })
      this._buttons.toggle.textContent = 'Stop'
      this._buttons.step.textContent = 'Switch to step'
    }
    if (this._state.status === 'step') {
      setAttr(this._buttons.toggle, { disabled: 1 })
      setAttr(this._buttons.step, { disabled: 1 })
      this._buttons.toggle.textContent = 'Run'
      this._buttons.step.textContent = '...'
    }
  }

  async toggleRunTest() {
    const status = await this._core.toggleRunTest()
  }

  async stepRunTest() {
    this.setRunStatus('step')
    await this._core.stepRunTest()
    this.setRunStatus('stop')
  }

  transformBody({body, transform}) {
    console.log('viewer@transformBody', body, transform)
    const bid = `body${body}`
    if (!this._sceneObjects[bid]) {
      console.error('cant find body to tranform', body)
      return
    }
    const mat = new THREE.Matrix4().fromArray(transform)
    // this._sceneObjects[bid].applyMatrix4(mat)
    this._sceneObjects[bid].matrix.copy(mat)
    this._sceneObjects[bid].matrix.decompose(this._sceneObjects[bid].position, this._sceneObjects[bid].quaternion, this._sceneObjects[bid].scale)
    this._sceneObjects[bid].updateMatrixWorld(true)
  }

  reshapeBody(body) {
    const bid = `body${body.id}`
    if (this._sceneObjects[bid]) {
      this._scene.remove(this._sceneObjects[bid])
      clearThree(this._sceneObjects[bid])
      delete this._sceneObjects[bid]
    }
    this.ioBodyCreated(body)
    console.log('viewer<reshapebody', bid, this._sceneObjects[bid])
  }
}
