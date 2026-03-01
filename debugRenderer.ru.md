DebugRenderer — как использовать                                                                                                                                                                                   
================================
                                                            
  world.debugDraw() собирает геометрию всех физических тел и ограничений из движка Jolt и возвращает её в виде TypedArray для рендеринга в Three.js / WebGL / Canvas.                                                
                                                            
  Возвращаемый объект                                                                                                                                                                                                
                                                            
  const geo = world.debugDraw({
    bodies: true,            // рисовать формы тел (default: true)
    constraints: false,      // рисовать ограничения (default: false)
    constraintLimits: false, // рисовать лимиты ограничений (default: false)
    wireframe: true,         // wireframe или solid (default: true)
  });

  // geo.lines         — Float32Array, по 6 float на отрезок: [x1,y1,z1, x2,y2,z2, ...]
  // geo.lineColors    — Uint32Array, 1 цвет (ARGB uint32) на отрезок
  // geo.triangles     — Float32Array, по 9 float на треугольник: [x1,y1,z1, x2,y2,z2, x3,y3,z3, ...]
  // geo.triangleColors — Uint32Array, 1 цвет (ARGB uint32) на треугольник

  Пример с Three.js (wireframe)

  import * as THREE from 'three';

  const scene = new THREE.Scene();
  const lineSegments = new THREE.LineSegments(
    new THREE.BufferGeometry(),
    new THREE.LineBasicMaterial({ vertexColors: true })
  );
  scene.add(lineSegments);

  function updateDebugDraw(world) {
    const geo = world.debugDraw({ wireframe: true });

    // --- Линии ---
    const lineCount = geo.lines.length / 6;
    const linePositions = geo.lines;                         // уже Float32Array
    const lineColors = new Float32Array(lineCount * 2 * 3);  // RGB на каждую вершину

    for (let i = 0; i < lineCount; i++) {
      const argb = geo.lineColors[i];
      const r = ((argb >> 16) & 0xff) / 255;
      const g = ((argb >>  8) & 0xff) / 255;
      const b = ((argb      ) & 0xff) / 255;
      // каждый отрезок — 2 вершины с одним цветом
      lineColors.set([r, g, b, r, g, b], i * 6);
    }

    const geom = lineSegments.geometry;
    geom.setAttribute('position', new THREE.BufferAttribute(linePositions, 3));
    geom.setAttribute('color',    new THREE.BufferAttribute(lineColors, 3));
    geom.computeBoundingSphere();
  }

  // В game loop:
  function animate() {
    world.step(1 / 60);
    updateDebugDraw(world);
    renderer.render(scene, camera);
    requestAnimationFrame(animate);
  }

  Пример solid-режим (треугольники)

  const mesh = new THREE.Mesh(
    new THREE.BufferGeometry(),
    new THREE.MeshBasicMaterial({ vertexColors: true, side: THREE.DoubleSide })
  );
  scene.add(mesh);

  function updateSolid(world) {
    const geo = world.debugDraw({ wireframe: false, bodies: true });

    const triCount = geo.triangles.length / 9;
    const colors = new Float32Array(triCount * 3 * 3); // RGB на каждую из 3 вершин

    for (let i = 0; i < triCount; i++) {
      const argb = geo.triangleColors[i];
      const r = ((argb >> 16) & 0xff) / 255;
      const g = ((argb >>  8) & 0xff) / 255;
      const b = ((argb      ) & 0xff) / 255;
      colors.set([r,g,b, r,g,b, r,g,b], i * 9);
    }

    const geom = mesh.geometry;
    geom.setAttribute('position', new THREE.BufferAttribute(geo.triangles, 3));
    geom.setAttribute('color',    new THREE.BufferAttribute(colors, 3));
    geom.computeBoundingSphere();
  }

  Цветовая схема Jolt

  По умолчанию (EShapeColor::MotionTypeColor) Jolt красит тела по типу движения:
  - серый — статические (NON_MOVING)
  - зелёный — активные динамические
  - синий — спящие

  Типичное использование

  Обычно debugDraw вызывается вместо или поверх игровой графики — для диагностики физики:

  // Рисовать wireframe поверх обычных мешей
  const debugMode = true;

  function render() {
    world.step(dt);

    if (debugMode) {
      updateDebugDraw(world); // показывает реальные физические формы
    }

    renderer.render(scene, camera);
  }

  Производительность

  debugDraw() — не дешёвый вызов: Jolt прогоняет всю сцену через DrawBodies. Вызывайте его не чаще раза в кадр и только когда нужна диагностика, а не в продакшне.

