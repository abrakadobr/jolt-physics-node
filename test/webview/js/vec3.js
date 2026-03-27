import { el, text } from 'redom'

export class Vec3Field {
  constructor() {
    this._value = { x: 0, y: 0, z: 0 }
    this._vecX = el('input.form-input', { type: 'numbr', step: 0.1, value: 0, placeholder: 'X' })
    this._vecY = el('input.form-input', { type: 'numbr', step: 0.1, value: 0, placeholder: 'Y' })
    this._vecZ = el('input.form-input', { type: 'numbr', step: 0.1, value: 0, placeholder: 'Z' })
    this._vecX.addEventListener('change', () => {
      console.log('X@change', this._vecX.value)
      this._value.x = this._vecX.value
    })
    this._vecY.addEventListener('change', () => {
      console.log('Y@change', this._vecY.value)
      this._value.y = this._vecY.value
    })
    this._vecZ.addEventListener('change', () => {
      console.log('Z@change', this._vecZ.value)
      this._value.z = this._vecZ.value
    })
    this._label = el('label')
    const f = el('div', [, el('div.d-flex.flex-row', [
      text('X'), vecX, text('Y'), vecY, text('Z'), vecZ
    ])])

    this._
  }
}
