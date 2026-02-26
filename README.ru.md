# jolt-physics-node

Нативный Node.js биндинг для [Jolt Physics](https://github.com/jrouwe/JoltPhysics) через Node-API (N-API).
Работает полностью на сервере — без WebAssembly и браузерного окружения.

> **English docs:** [README.md](./README.md)

---

## Возможности

- Симуляция твёрдых тел — сферы, боксы, капсулы, цилиндры, выпуклые оболочки, меши, поля высот, составные фигуры
- Полная система ограничений — шарнир, слайдер, точка, конус, фиксированный, дистанция, swing-twist, 6DOF, шестерня, блок-и-такль, рейка-и-шестерня, путевой констрейнт
- Моторы ограничений с параметрами пружины и демпфирования
- Пространственные запросы — рейкасты, коллизии/свипы сферой, AABB-пересечения
- Материальные индексы (трение, упругость) для отдельных треугольников меша и ячеек поля высот
- Система скелета и рэгдолла с настройкой формы и ограничений каждого сустава
- Компактные бинарные снепшоты состояния для сетевой синхронизации и воспроизведения
- Полное сохранение/загрузка сцены (формат Jolt `PhysicsScene`)
- `PhysicsWorker` — мир в отдельном потоке Worker с асинхронным API
- Колбэки событий контактов и активации тел

---

## Требования

- Node.js 18+
- Тулчейн C++17 (`g++` или `clang++`, `make`, Python 3 для `node-gyp`)

---

## Установка

```bash
git clone --recurse-submodules <repo-url>
npm install        # автоматически собирает нативный аддон
```

Если клонировали без `--recurse-submodules`:

```bash
git submodule update --init
npm install
```

---

## Быстрый старт

```js
const { World } = require('jolt-physics-node');

const world = new World({ gravity: 9.81 });

const ball = world.createSphere({
  radius: 0.5,
  position: { x: 0, y: 5, z: 0 },
  dynamic: true,
  restitution: 0.4,
  friction: 0.5,
});

for (let i = 0; i < 120; i++) world.step(1 / 60);

console.log(world.getBodyPosition(ball)); // y ≈ 0

world.destroy();
```

---

## Справочник API

### World

```js
const world = new World({ gravity?: number });  // гравитация по умолчанию 9.81 м/с²
world.step(dt);          // шаг симуляции
world.setGravity(9.81);
world.destroy();         // освободить все ресурсы
```

При создании `World` автоматически добавляется статическая земля на `y = -1`.

---

### Создание тел

Все методы создания возвращают числовой `BodyId`. Общие опции для любой формы:

| Параметр | Тип | По умолч. | Описание |
|---|---|---|---|
| `position` | `Vec3` | обязательно | Позиция в мировом пространстве |
| `dynamic` | `boolean` | `true` | `false` = статичное тело |
| `friction` | `number` | `0.5` | Коэффициент трения |
| `restitution` | `number` | `0.2` | Упругость (отскок) |

```js
world.createSphere({ radius, position, dynamic?, friction?, restitution? })
world.createBox({ halfExtents: Vec3, position, ... })
world.createCapsule({ halfHeight, radius, position, ... })
world.createCylinder({ halfHeight, radius, position, ... })
world.createTaperedCapsule({ halfHeight, topRadius, bottomRadius, position, ... })
world.createTaperedCylinder({ halfHeight, topRadius, bottomRadius, position, ... })
world.createConvexHull({ points: number[], position, ... })  // плоский массив [x,y,z,...], ≥4 точки
```

**Меш** (только статичный):

```js
world.createMesh({
  vertices: number[],   // плоский массив [x,y,z, ...]
  indices:  number[],   // список треугольников [i0,i1,i2, ...]
  position: Vec3,
  friction?: number,
  restitution?: number,
  materialIndices?: number[] | Uint32Array,  // один индекс на треугольник
  materials?: Array<{ friction?: number, restitution?: number }>,
})
```

**Поле высот** (статичный ландшафт):

```js
world.createHeightField({
  samples: Float32Array | number[],  // N×N значений высот, построчно
  sampleCount: number,               // N (≥2, например 64)
  offset?: Vec3,
  scale?: Vec3,
  position?: Vec3,
  friction?: number,
  restitution?: number,
  materialIndices?: Uint8Array | number[],  // один на ячейку
  materials?: Array<{ friction?: number, restitution?: number }>,
})
```

**Составные фигуры:**

```js
// Статичный compound — несколько форм, запечённых в одно тело
world.createStaticCompound({
  shapes: SubShapeSpec[],
  position: Vec3,
  dynamic?: boolean,
  friction?: number,
  restitution?: number,
})

// Mutable compound — подформы можно добавлять/удалять в рантайме
const id = world.createMutableCompound({ shapes, position, dynamic?, ... })
const idx = world.addMutableSubShape(id, spec)
world.removeMutableSubShape(id, idx)
world.modifyMutableSubShape(id, idx, position, rotation?)
world.adjustMutableCenterOfMass(id)  // вызвать после любых изменений
```

`SubShapeSpec` — `{ kind: 'sphere'|'box'|'capsule'|'cylinder', position?, rotation?, radius?, halfHeight?, halfExtents? }`

---

### Состояние тела

```js
// Позиция / вращение
world.getBodyPosition(id)              // → Vec3
world.getBodyRotation(id)              // → Quat
world.getCenterOfMassPosition(id)      // → Vec3
world.setBodyPosition(id, pos, activate = true)
world.setBodyRotation(id, quat, activate = true)

// Скорости
world.getLinearVelocity(id)            // → Vec3 (м/с)
world.getAngularVelocity(id)           // → Vec3 (рад/с)
world.setLinearVelocity(id, vec3)
world.setAngularVelocity(id, vec3)

// Силы и импульсы
world.applyImpulse(id, vec3)           // кг·м/с
world.addAngularImpulse(id, vec3)
world.addForce(id, vec3)               // Н (сбрасывается каждый шаг)
world.addTorque(id, vec3)              // Н·м

// Материальные свойства
world.getFriction(id) / world.setFriction(id, v)
world.getRestitution(id) / world.setRestitution(id, v)

// Динамика
world.getMotionType(id) / world.setMotionType(id, type, activate?)
// type: 0=Static, 1=Kinematic, 2=Dynamic
world.getMotionQuality(id) / world.setMotionQuality(id, quality)
// quality: 0=Discrete, 1=LinearCast (CCD)
world.getGravityFactor(id) / world.setGravityFactor(id, v)  // 0 = без гравитации
world.getObjectLayer(id) / world.setObjectLayer(id, layer)  // 0=NON_MOVING, 1=MOVING
world.getDamping(id) / world.setDamping(id, { linear, angular })

// Жизненный цикл
world.activateBody(id)
world.deactivateBody(id)
world.removeBody(id)

// Предикаты (возвращают boolean, не бросают исключений)
world.hasBody(id)
world.isBodyActive(id)
world.isBodySensor(id)
world.areBodiesInContact(idA, idB)
world.setBodySensor(id, bool)   // триггерная зона — детектирует перекрытия, без сил
```

---

### Пространственные запросы

Все запросы принимают необязательный `filter`:

```js
filter?: {
  layerMask?: number,         // битовая маска — бит N = разрешить слой N (по умолч. все)
  excludeBodyIds?: number[],  // игнорировать конкретные тела
}
```

```js
// Ближайшее попадание луча — null при промахе
const hit = world.rayCastClosest({
  origin: Vec3, direction: Vec3, maxDistance: number, filter?
})
// → { bodyId, fraction, normal: Vec3, materialIndex } | null

// Все попадания по дистанции
const hits = world.rayCastAll({ origin, direction, maxDistance, filter? })
// → [{ bodyId, fraction, normal, materialIndex }]

// Тела, перекрывающие сферу
const overlaps = world.collideSphereAll({
  center: Vec3, radius: number, maxSeparation?: number, filter?
})
// → [{ bodyId, contactPoint: Vec3, penetrationDepth, normal: Vec3, materialIndex }]

// Свип сферы вдоль луча
const sweeps = world.castSphereAll({
  origin: Vec3, direction: Vec3, maxDistance: number, radius: number, filter?
})
// → [{ bodyId, fraction, penetrationDepth, point: Vec3, normal: Vec3, materialIndex }]

// Перекрытие AABB — возвращает только идентификаторы
const bodies = world.queryAABB({ min: Vec3, max: Vec3, filter? })
// → [{ bodyId }]
```

`materialIndex` равен 0 если форма создана без материальных индексов, иначе — индекс в массив `materials`, переданный при создании меша или поля высот.

---

### Ограничения (Constraints)

Все конструкторы возвращают числовой `ConstraintId`. Неверный ID бросает исключение в сеттерах и геттерах.

```js
world.createFixedConstraint(bodyA, bodyB)

world.createDistanceConstraint(bodyA, bodyB, pointA, pointB, minDist?, maxDist?)
world.setDistanceLimits(id, min, max)
world.getDistanceLimits(id)            // → { min, max }
world.setDistanceLimitsSpring(id, springOpts)
world.getDistanceLimitsSpring(id)

world.createHingeConstraint(bodyA, bodyB, anchor, axis, normal)
world.setHingeLimits(id, minRad, maxRad)
world.getHingeLimits(id)               // → { min, max }
world.getHingeAngle(id)                // → радианы
world.setHingeMotor(id, motorOpts)
world.getHingeMotorState(id)

world.createSliderConstraint(bodyA, bodyB, anchor, axis, normal, min?, max?)
world.setSliderLimits(id, min, max)
world.getSliderLimits(id)              // → { min, max }
world.getSliderPosition(id)            // → метры
world.setSliderMotor(id, motorOpts)
world.getSliderMotorState(id)

world.createPointConstraint(bodyA, bodyB, pivotWorld)

world.createConeConstraint(bodyA, bodyB, pivot, twistAxis, halfConeAngle)
world.setConeHalfAngle(id, radians)

world.createSwingTwistConstraint(bodyA, bodyB, pivot, twistAxis, planeAxis, limits?)
// limits: { normalHalfCone, planeHalfCone, twistMin, twistMax }
world.setSwingTwistLimits(id, limits)
world.getSwingTwistLimits(id)
world.getSwingTwistRotation(id)        // → Quat
world.setSwingTwistMotor(id, motorOpts)
world.getSwingTwistMotorState(id)

world.createSixDOFConstraint(bodyA, bodyB, pivot, axisX, axisY)
world.setSixDOFLimits(id, {
  translationMin: Vec3, translationMax: Vec3,
  rotationMin: Vec3, rotationMax: Vec3,
})
world.getSixDOFLimits(id)
world.getSixDOFRotation(id)            // → Vec3 (углы Эйлера)
world.setSixDOFMotorState(id, axis, state)   // axis 0–5
world.getSixDOFMotorState(id, axis)
world.setSixDOFTargetVelocity(id, linVec3, angVec3)
world.setSixDOFTargetPose(id, posVec3, rotQuat)

world.createGearConstraint(bodyA, bodyB, hingeAxis1, hingeAxis2,
                            ratio?, hinge1Id?, hinge2Id?)

world.createPulleyConstraint(bodyA, bodyB,
  bodyPoint1, fixedPoint1, bodyPoint2, fixedPoint2,
  { ratio?, minLength?, maxLength? })  // maxLength=-1 = авторасчёт
world.getPulleyLength(id)              // → текущая длина верёвки
world.getPulleyLengthLimits(id)        // → { min, max }
world.setPulleyLength(id, min, max)

world.createRackAndPinionConstraint(bodyA, bodyB, {
  hingeAxis, sliderAxis, ratio,
  pinionConstraintId?, rackConstraintId?,
})

world.createPathConstraint(bodyA, bodyB, {
  points: Array<{ position, tangent, normal }>,  // Vec3
  closed?, pathPosition?, pathRotation?,
  pathFraction?, maxFriction?,
  rotationType?: 'free'|'tangent'|'normal'|'binormal'|'toPath'|'full',
})
world.getPathFraction(id)             // → 0 до N-1
world.getPathMaxFraction(id)          // → N-1
world.setPathMotor(id, { state, targetVelocity?, targetFraction? })
world.getPathMotorState(id)

world.removeConstraint(id)
```

#### Параметры мотора

```js
{
  state: 0 | 1 | 2,       // 0=Off, 1=Velocity, 2=Position
  targetVelocity?: number,
  targetAngle?: number,    // Hinge — радианы
  targetPosition?: number, // Slider — метры
  maxTorque?: number,
  maxForce?: number,
}
```

#### Пружина и демпфирование мотора

Доступно для: Hinge, Slider, SwingTwist (swing и twist отдельно), SixDOF (по оси), Path, Distance limits.

```js
world.setHingeMotorSpring(id, {
  mode?: 0 | 1,       // 0=FrequencyAndDamping (по умолч.), 1=StiffnessAndDamping
  frequency?: number, // Гц  (mode 0)
  stiffness?: number, // Н/м (mode 1)
  damping?: number,
  maxTorque?: number,
  minTorque?: number,
  maxForce?: number,
  minForce?: number,
})

world.getHingeMotorSpring(id)
// → { mode, frequency, damping, minForceLimit, maxForceLimit, minTorqueLimit, maxTorqueLimit }

// Аналогично для остальных типов:
world.setSliderMotorSpring(id, opts)
world.setSwingMotorSpring(id, opts)
world.setTwistMotorSpring(id, opts)
world.setSixDOFMotorSpring(id, axis, opts)  // axis 0–5
world.setPathMotorSpring(id, opts)
world.setDistanceLimitsSpring(id, opts)
```

#### Лямбды (импульсы) ограничений

Импульсы, приложенные ограничением на последнем шаге симуляции — полезно для мониторинга нагрузок.

```js
world.getHingeLambdas(id)      // → { position: Vec3, rotation: Vec2, rotationLimits, motor }
world.getSliderLambdas(id)     // → { position: Vec2, positionLimits, rotation: Vec3, motor }
world.getSwingTwistLambdas(id) // → { position: Vec3, twist, swingY, swingZ, motor: Vec3 }
world.getSixDOFLambdas(id)     // → { position, rotation, motorTranslation, motorRotation } (все Vec3)
world.getConeLambdas(id)       // → { position: Vec3, rotation }
world.getPointLambdas(id)      // → { position: Vec3 }
world.getFixedLambdas(id)      // → { position: Vec3, rotation: Vec3 }
world.getDistanceLambda(id)    // → number
world.getPulleyLambda(id)      // → number
world.getGearLambda(id)        // → number
world.getPathLambdas(id)       // → { position: Vec2, positionLimits, motor, rotationHinge: Vec2, rotation: Vec3 }
```

---

### Скелет и рэгдолл

```js
const skeleton = world.createSkeleton();
skeleton.addJoint('root', -1);      // имя, индекс родителя (-1 = корень)
skeleton.addJoint('spine', 0);
skeleton.finalize();

skeleton.getJointCount()
skeleton.getJointIndex('spine')     // → number | -1
skeleton.getJointInfo(0)            // → { name, parentIndex }

const settings = skeleton.createRagdollSettings({
  capsuleHalfHeight?: number,   // по умолч. 0.2
  capsuleRadius?: number,       // по умолч. 0.1
  spacing?: number,             // расстояние между суставами, по умолч. 0.4
});

// Переопределить форму конкретного сустава
settings.setJointShape(index, {
  kind: 'capsule' | 'box' | 'sphere',
  halfHeight?, radius?, halfExtents?
})

// Переопределить трансформ сустава (в мировом пространстве)
settings.setJointTransform(index, position, rotation)

// Тип ограничения между суставом и его родителем
settings.setJointConstraint(index, {
  type: 'fixed' | 'swingTwist' | 'hinge' | 'cone',
  autoAxes?: boolean,          // автоматически вычислить оси из направления кости (по умолч. true)
  // SwingTwist:
  normalHalfCone?, planeHalfCone?, twistMin?, twistMax?,
  // Hinge:
  minAngle?, maxAngle?,
  // Cone:
  halfConeAngle?,
})

const ragdoll = settings.createRagdoll({
  collisionGroup?: number,
  userData?: number,
  activate?: boolean,
})

ragdoll.bodyCount()
ragdoll.getBoneBodyId(index)          // → BodyId — использовать с методами world.*
ragdoll.getBoneTransform(index)       // → { position: Vec3, rotation: Quat }
ragdoll.setBoneTransform(index, transform)
ragdoll.syncToSkeletonPose()          // → Transform[] — снепшот всех костей
ragdoll.syncFromSkeletonPose(poses)   // применить массив поз
ragdoll.getConstraintIds()            // → number[] — использовать с setSwingTwistMotor и т.д.
ragdoll.destroy()
```

---

### События

```js
world.onBodyActivation((event) => {
  // event: { type: 'activated'|'deactivated', bodyId }
})

world.onContact((event) => {
  // event.type: 'added' | 'persisted' | 'removed'
  // Для 'added' / 'persisted': + bodyA, bodyB, point, normal, penetrationDepth
  // Для 'removed':             + bodyA, bodyB
})
```

---

### Сериализация

```js
// Компактный снепшот: позиция + вращение + скорости всех тел
// 56 байт на тело — подходит для сетевой синхронизации и интерполяции
const buf = world.snapshotState()      // → Buffer
world.applySnapshot(buf)               // применить к существующим телам по ID

// Полная сцена (формат Jolt PhysicsScene)
// Включает геометрию форм + текущий трансформ + скорости
// Внимание: пользовательские ограничения НЕ сохраняются — пересоздайте их после loadScene()
const sceneBuf = world.saveScene()     // → Buffer
const count = world.loadScene(buf)     // → количество восстановленных тел
```

---

### PhysicsWorker

Запускает `World` в отдельном потоке `worker_threads`. Все методы `World` доступны как `async`-эквиваленты.

```js
const { PhysicsWorker } = require('jolt-physics-node');

const worker = await PhysicsWorker.create(
  { gravity: 9.81 },        // опции World
  { autoState: true },      // отправлять снепшот после каждого step (по умолч. true)
);

worker.onState((snapshot) => {
  // Buffer — тот же формат 56 байт/тело что и snapshotState()
  // передаётся из воркера без копирования (zero-copy transfer)
});

worker.onEvent((kind, data) => {
  // kind: 'bodyActivation' | 'contact'
});

const ballId = await worker.createSphere({ radius: 0.5, position: {x:0,y:5,z:0} });
await worker.step(1 / 60);

const pos = await worker.getBodyPosition(ballId);
await worker.applyImpulse(ballId, { x: 0, y: 10, z: 0 });

const hit = await worker.rayCastClosest({
  origin: {x:0,y:10,z:0}, direction: {x:0,y:-1,z:0}, maxDistance: 30
});

// Сериализация
const buf = await worker.snapshotState();
await worker.applySnapshot(buf);

// Завершение работы: отправляет 'destroy' с таймаутом 2с,
// отклоняет все ожидающие вызовы и завершает поток
await worker.terminate();
```

Все методы `World` доступны как асинхронные методы `PhysicsWorker` с идентичными сигнатурами.

---

### Обработка ошибок

| Ситуация | Поведение |
|---|---|
| Неверный `bodyId` / `constraintId` в сеттере или действии | бросает `Error` |
| Неверный `bodyId` / `constraintId` в геттере | бросает `Error` |
| `rayCastClosest` — нет попадания | возвращает `null` |
| `rayCastAll`, `collideSphereAll` и т.д. — нет попаданий | возвращает `[]` |
| `hasBody`, `isBodyActive`, `isBodySensor`, `areBodiesInContact` | возвращает `boolean`, никогда не бросает |

---

## Примеры

```bash
npm run examples
```

Открывает интерактивное демо по адресу `http://127.0.0.1:8787` со сценариями:
падающие сферы, ограничения, выпуклые оболочки, меш-тела, скелет-рэгдолл.

---

## Сборка из исходников

```bash
npm run build    # node-gyp rebuild
npm test         # smoke-тест
```

JoltPhysics подключён как git submodule — отдельная установка не нужна.

---

## Лицензия

MIT
