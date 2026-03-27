import EE from '../js/ee.js'

console.log('EE', EE)

export class DropABallTest extends EE {

  constructor(core, sidebar, viewer) {
    super()
    this._core = core
    this._sidebar = sidebar
    this._viewer = viewer
    this._stage = ''
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

  load(sidebar) {
    this.emit('load:start')
    // console.log('LOAD', sidebar)
    // sidebar.update(false)

    // this.emit('load:end')
  }

  reset() {
    this.emit('reset')
  }

  configure(sidebar) {
    this.emit('configure:start')

    this.emit('configure:end')
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
