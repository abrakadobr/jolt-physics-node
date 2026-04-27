import EE from 'ee'

export class DropABallTest extends EE {
  static info() {
    return {
      code: 'dropABall',
      name: 'Drop a ball',
      inputs: {},
      stages: {}
    }
  }

  constructor(core, viewer, sidebar) {
    super()
    this._core = core
    this._viewer = viewer
    this._sidebar = sidebar
  }

  abortAndClose() {}
  reset(values) {}
  configure(wio, fcode, values) {}

  load(wio) {
    this.emit('load:end')
  }
}
