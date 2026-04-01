import EE from '../js/ee.js'

export class DropABallTest extends EE {

  constructor(core, sidebar, viewer) {
    super()
    this._core = core
    this._sidebar = sidebar
    this._viewer = viewer
    this._stage = ''
    this._values = {}
    this._bodies = {
      box: null,
      ball: null
    }
  }

  static info() {
    return {
      code: 'dropABall',
      name: 'Drop a ball',
      stages: {
        load: true,
        configure: true,
        run: true,
        infinite: false,
        autostop: true
      },
      inputs: {
        position: {
          type: 'Vec3',
          stages: ['configure'],
          name: 'Ball initial position',
          default: { x: 0, y: 3, z: 0 }
        },
        radius: {
          type: 'float',
          stages: ['configure'],
          name: 'Ball radius',
          default: 1
        }
      }
    }
  }

  async load(wio) {
    console.log('dropABall@load:start')
    await wio.defineStandartLayers()
    console.log('dropABall@load:layersDefined')
    const boxId = await wio.createBody({
      type: 'box',
      shape: { halfExtent : { x: 10, y: 0.5, z: 10 }, convexRadius: 10},
      settings: {
        position: { x: 0, y: -0.5, z: 0 },
        rotation: { x: 0, y : 0, z: 0, w: 1},
        active: true,
        motionType: 'static',
        layer: 'static'
      }
    })
    console.log('dropABall@load:box')
    const ballId = await wio.createBody({
      type: 'sphere',
      shape: { radius: this._values.radius},
      settings: {
        position: this._values.position,
        rotation: { x: 0, y : 0, z: 0, w: 1},
        active: true,
        motionType: 'dynamic',
        layer: 'dynamic'
      }
    })
    console.log('dropABall@load:ball')
    const todel = [this._bodies.box, this._bodies.ball].filter(x => !!x)
    console.log('dropABall@load:todel', todel.length)
    if (todel.length) await wio.deleteBodies(todel)
    console.log('dropABall@load:todel.done', todel.length)
    this._bodies.box = boxId
    this._bodies.ball = ballId
    console.log('dropABall@load:end', this._bodies)
    this.emit('load:end')
  }

  reset(values) {
    this._values = values
  }

  async configure(wio, fcode, values) {
    if (fcode === 'position') {
      await wio.setBodyPosition( this._bodies.ball, values.position)
    }
    if (fcode === 'radius') {
      await wio.reshapeBody(this._bodies.ball, { type: 'sphere', shape: { radius: values.radius }})
    }
    // this.emit('configure:start')

    // this.emit('configure:end')
  }

  run(sidebar) {
    this.emit('run:start')

    this.emit('run:end')
  }

  step(sidebar) {
    this.emit('step:start')

    this.emit('step:end')
  }

  abortAndClose() {
    this.emit('abortandclose:start')

    this.emit('abortandclose:end')
  }
}
