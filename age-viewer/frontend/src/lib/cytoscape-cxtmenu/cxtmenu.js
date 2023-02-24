/*
 * Copyright (c) 2016-2020, The Cytoscape Consortium.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the “Software”), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/* eslint-disable */
const defaults = require('./defaults');
const assign = require('./assign');
const {
  removeEles, setStyles, createElement, getPixelRatio, getOffset,
} = require('./dom-util');

const cxtmenu = function (params) {
  const options = assign({}, defaults, params);
  const cy = this;
  const container = cy.container();
  let target;

  const data = {
    options,
    handlers: [],
    container: createElement({ class: 'cxtmenu' }),
  };

  const wrapper = data.container;
  const parent = createElement();
  const canvas = createElement({ tag: 'canvas' });
  let commands = [];
  const c2d = canvas.getContext('2d');

  let r = 100; // default radius
  let containerSize = (r + options.activePadding) * 2;
  let activeCommandI;
  let offset;

  container.insertBefore(wrapper, container.firstChild);
  wrapper.appendChild(parent);
  parent.appendChild(canvas);

  setStyles(wrapper, {
    position: 'absolute',
    zIndex: options.zIndex,
    userSelect: 'none',
    pointerEvents: 'none', // prevent events on menu in modern browsers
  });

  // prevent events on menu in legacy browsers
  ['mousedown', 'mousemove', 'mouseup', 'contextmenu'].forEach((evt) => {
    wrapper.addEventListener(evt, (e) => {
      e.preventDefault();

      return false;
    });
  });

  setStyles(parent, {
    display: 'none',
    width: `${containerSize}px`,
    height: `${containerSize}px`,
    position: 'absolute',
    zIndex: 1,
    marginLeft: `${-options.activePadding}px`,
    marginTop: `${-options.activePadding}px`,
    userSelect: 'none',
  });

  canvas.width = containerSize;
  canvas.height = containerSize;

  function createMenuItems() {
    removeEles('.cxtmenu-item', parent);
    const dtheta = 2 * Math.PI / (commands.length);
    let theta1 = Math.PI / 2;
    let theta2 = theta1 + dtheta;

    for (let i = 0; i < commands.length; i++) {
      const command = commands[i];

      const midtheta = (theta1 + theta2) / 2;
      const rx1 = 0.66 * r * Math.cos(midtheta);
      const ry1 = 0.66 * r * Math.sin(midtheta);

      const item = createElement({ class: 'cxtmenu-item' });
      setStyles(item, {
        color: options.itemColor,
        cursor: 'default',
        display: 'table',
        'text-align': 'center',
        // background: 'red',
        position: 'absolute',
        'text-shadow': `-1px -1px 2px ${options.itemTextShadowColor}, 1px -1px 2px ${options.itemTextShadowColor}, -1px 1px 2px ${options.itemTextShadowColor}, 1px 1px 1px ${options.itemTextShadowColor}`,
        left: '50%',
        top: '50%',
        'min-height': `${r * 0.66}px`,
        width: `${r * 0.66}px`,
        height: `${r * 0.66}px`,
        marginLeft: `${rx1 - r * 0.33}px`,
        marginTop: `${-ry1 - r * 0.33}px`,
      });

      const content = createElement({ class: 'cxtmenu-content' });

      if (command.content instanceof HTMLElement) {
        content.appendChild(command.content);
      } else {
        content.innerHTML = command.content;
      }

      setStyles(content, {
        width: `${r * 0.66}px`,
        height: `${r * 0.66}px`,
        'vertical-align': 'middle',
        display: 'table-cell',
      });

      setStyles(content, command.contentStyle || {});

      if (command.disabled === true || command.enabled === false) {
        content.setAttribute('class', 'cxtmenu-content cxtmenu-disabled');
      }

      parent.appendChild(item);
      item.appendChild(content);

      theta1 += dtheta;
      theta2 += dtheta;
    }
  }

  function queueDrawBg(radius, rspotlight) {
    redrawQueue.drawBg = [radius, rspotlight];
  }

  function drawBg(radius, rspotlight) {
    rspotlight = rspotlight !== undefined ? rspotlight : rs;
    c2d.globalCompositeOperation = 'source-over';

    c2d.clearRect(0, 0, containerSize, containerSize);

    // draw background items
    c2d.fillStyle = options.fillColor;
    const dtheta = 2 * Math.PI / (commands.length);
    let theta1 = Math.PI / 2;
    let theta2 = theta1 + dtheta;

    for (let index = 0; index < commands.length; index++) {
      const command = commands[index];

      if (command.fillColor) {
        c2d.fillStyle = command.fillColor;
      }
      c2d.beginPath();
      c2d.moveTo(radius + options.activePadding, radius + options.activePadding);
      c2d.arc(radius + options.activePadding, radius + options.activePadding, radius, 2 * Math.PI - theta1, 2 * Math.PI - theta2, true);
      c2d.closePath();
      c2d.fill();

      theta1 += dtheta;
      theta2 += dtheta;

      c2d.fillStyle = options.fillColor;
    }

    // draw separators between items
    c2d.globalCompositeOperation = 'destination-out';
    c2d.strokeStyle = 'white';
    c2d.lineWidth = options.separatorWidth;
    theta1 = Math.PI / 2;
    theta2 = theta1 + dtheta;

    for (let i = 0; i < commands.length; i++) {
      const rx1 = radius * Math.cos(theta1);
      const ry1 = radius * Math.sin(theta1);
      c2d.beginPath();
      c2d.moveTo(radius + options.activePadding, radius + options.activePadding);
      c2d.lineTo(radius + options.activePadding + rx1, radius + options.activePadding - ry1);
      c2d.closePath();
      c2d.stroke();

      theta1 += dtheta;
      theta2 += dtheta;
    }

    c2d.fillStyle = 'white';
    c2d.globalCompositeOperation = 'destination-out';
    c2d.beginPath();
    c2d.arc(radius + options.activePadding, radius + options.activePadding, rspotlight + options.spotlightPadding, 0, Math.PI * 2, true);
    c2d.closePath();
    c2d.fill();

    c2d.globalCompositeOperation = 'source-over';
  }

  function queueDrawCommands(rx, ry, radius, theta) {
    redrawQueue.drawCommands = [rx, ry, radius, theta];
  }

  function drawCommands(rx, ry, radius, theta) {
    const dtheta = 2 * Math.PI / (commands.length);
    let theta1 = Math.PI / 2;
    let theta2 = theta1 + dtheta;

    theta1 += dtheta * activeCommandI;
    theta2 += dtheta * activeCommandI;

    c2d.fillStyle = options.activeFillColor;
    c2d.strokeStyle = 'black';
    c2d.lineWidth = 1;
    c2d.beginPath();
    c2d.moveTo(r + options.activePadding, r + options.activePadding);
    c2d.arc(r + options.activePadding, r + options.activePadding, r + options.activePadding, 2 * Math.PI - theta1, 2 * Math.PI - theta2, true);
    c2d.closePath();
    c2d.fill();

    c2d.fillStyle = 'white';
    c2d.globalCompositeOperation = 'destination-out';

    const tx = radius + options.activePadding + rx / radius * (rs + options.spotlightPadding - options.indicatorSize / 4);
    const ty = radius + options.activePadding + ry / radius * (rs + options.spotlightPadding - options.indicatorSize / 4);
    const rot = Math.PI / 4 - theta;

    c2d.translate(tx, ty);
    c2d.rotate(rot);

    // clear the indicator
    c2d.beginPath();
    c2d.fillRect(-options.indicatorSize / 2, -options.indicatorSize / 2, options.indicatorSize, options.indicatorSize);
    c2d.closePath();
    c2d.fill();

    c2d.rotate(-rot);
    c2d.translate(-tx, -ty);

    // c2d.setTransform( 1, 0, 0, 1, 0, 0 );

    // clear the spotlight
    c2d.beginPath();
    c2d.arc(r + options.activePadding, radius + options.activePadding, rs + options.spotlightPadding, 0, Math.PI * 2, true);
    c2d.closePath();
    c2d.fill();

    c2d.globalCompositeOperation = 'source-over';
  }

  function updatePixelRatio() {
    const pxr = getPixelRatio();
    const w = containerSize;
    const h = containerSize;

    canvas.width = w * pxr;
    canvas.height = h * pxr;

    canvas.style.width = `${w}px`;
    canvas.style.height = `${h}px`;

    c2d.setTransform(1, 0, 0, 1, 0, 0);
    c2d.scale(pxr, pxr);
  }

  let redrawing = true;
  let redrawQueue = {};

  const raf = (
    window.requestAnimationFrame
    || window.webkitRequestAnimationFrame
    || window.mozRequestAnimationFrame
    || window.msRequestAnimationFrame
    || ((fn) => setTimeout(fn, 16))
  );

  const redraw = function () {
    if (redrawQueue.drawBg) {
      drawBg.apply(null, redrawQueue.drawBg);
    }

    if (redrawQueue.drawCommands) {
      drawCommands.apply(null, redrawQueue.drawCommands);
    }

    redrawQueue = {};

    if (redrawing) {
      raf(redraw);
    }
  };

  // kick off
  updatePixelRatio();
  redraw();

  let ctrx; let ctry; let
    rs;

  const bindings = {
    on(events, selector, fn) {
      let _fn = fn;
      if (selector === 'core') {
        _fn = function (e) {
          if (e.cyTarget === cy || e.target === cy) { // only if event target is directly core
            return fn.apply(this, [e]);
          }
        };
      }

      data.handlers.push({
        events,
        selector,
        fn: _fn,
      });

      if (selector === 'core') {
        cy.on(events, _fn);
      } else {
        cy.on(events, selector, _fn);
      }

      return this;
    },
  };

  function addEventListeners() {
    let grabbable;
    let inGesture = false;
    let dragHandler;
    let zoomEnabled;
    let panEnabled;
    let boxEnabled;
    let gestureStartEvent;

    const restoreZoom = function () {
      if (zoomEnabled) {
        cy.userZoomingEnabled(true);
      }
    };

    const restoreGrab = function () {
      if (grabbable) {
        target.grabify();
      }
    };

    const restorePan = function () {
      if (panEnabled) {
        cy.userPanningEnabled(true);
      }
    };

    const restoreBoxSeln = function () {
      if (boxEnabled) {
        cy.boxSelectionEnabled(true);
      }
    };

    const restoreGestures = function () {
      restoreGrab();
      restoreZoom();
      restorePan();
      restoreBoxSeln();
    };

    window.addEventListener('resize', updatePixelRatio);

    bindings
      .on('resize', () => {
        updatePixelRatio();
      })

      .on(options.openMenuEvents, options.selector, function (e) {
        target = this; // Remember which node the context menu is for
        const ele = this;
        const isCy = this === cy;

        if (inGesture) {
          parent.style.display = 'none';

          inGesture = false;

          restoreGestures();
        }

        if (typeof options.commands === 'function') {
          const res = options.commands(target);
          if (res.then) {
            res.then((_commands) => {
              commands = _commands;
              openMenu();
            });
          } else {
            commands = res;
            openMenu();
          }
        } else {
          commands = options.commands;
          openMenu();
        }

        function openMenu() {
          if (!commands || commands.length === 0) { return; }

          zoomEnabled = cy.userZoomingEnabled();
          cy.userZoomingEnabled(false);

          panEnabled = cy.userPanningEnabled();
          cy.userPanningEnabled(false);

          boxEnabled = cy.boxSelectionEnabled();
          cy.boxSelectionEnabled(false);

          grabbable = target.grabbable && target.grabbable();
          if (grabbable) {
            target.ungrabify();
          }

          let rp; let rw; let
            rh;
          if (!isCy && ele.isNode() && !ele.isParent() && !options.atMouse) {
            rp = ele.renderedPosition();
            rw = ele.renderedOuterWidth();
            rh = ele.renderedOuterHeight();
          } else {
            rp = e.renderedPosition || e.cyRenderedPosition;
            rw = 1;
            rh = 1;
          }

          offset = getOffset(container);

          ctrx = rp.x;
          ctry = rp.y;

          r = rw / 2 + options.menuRadius(target);
          containerSize = (r + options.activePadding) * 2;
          updatePixelRatio();

          setStyles(parent, {
            width: `${containerSize}px`,
            height: `${containerSize}px`,
            display: 'block',
            left: `${rp.x - r}px`,
            top: `${rp.y - r}px`,
          });
          createMenuItems();

          rs = Math.max(rw, rh) / 2;
          rs = Math.max(rs, options.minSpotlightRadius);
          rs = Math.min(rs, options.maxSpotlightRadius);

          queueDrawBg(r);

          activeCommandI = undefined;

          inGesture = true;
          gestureStartEvent = e;
        }
      })

      .on('cxtdrag tapdrag', options.selector, dragHandler = function (e) {
        if (!inGesture) { return; }

        const origE = e.originalEvent;
        const isTouch = origE.touches && origE.touches.length > 0;

        const pageX = (isTouch ? origE.touches[0].pageX : origE.pageX) - window.pageXOffset;
        const pageY = (isTouch ? origE.touches[0].pageY : origE.pageY) - window.pageYOffset;

        activeCommandI = undefined;

        // refresh offset, because of scroll or resizing..
        offset = getOffset(container);

        let dx = pageX - offset.left - ctrx;
        const dy = pageY - offset.top - ctry;

        if (dx === 0) { dx = 0.01; }

        const d = Math.sqrt(dx * dx + dy * dy);
        const cosTheta = (dy * dy - d * d - dx * dx) / (-2 * d * dx);
        let theta = Math.acos(cosTheta);

        let rw;
        if (target && typeof target.isNode === 'function' && target.isNode() && !target.isParent() && !options.atMouse) {
          rw = target.renderedOuterWidth();
        } else {
          rw = 1;
        }

        r = rw / 2 + options.menuRadius(target);
        if (d < rs + options.spotlightPadding) {
          queueDrawBg(r);
          return;
        }
        queueDrawBg(r);

        const rx = dx * r / d;
        const ry = dy * r / d;

        if (dy > 0) {
          theta = Math.PI + Math.abs(theta - Math.PI);
        }

        const dtheta = 2 * Math.PI / (commands.length);
        let theta1 = Math.PI / 2;
        let theta2 = theta1 + dtheta;

        const isMouseCircleIn = Math.pow(r, 2) > (Math.pow(ctrx - (pageX - offset.left), 2) + Math.pow(ctry - (pageY - offset.top), 2));

        for (let i = 0; i < commands.length; i++) {
          const command = commands[i];

          let inThisCommand = theta1 <= theta && theta <= theta2
            || theta1 <= theta + 2 * Math.PI && theta + 2 * Math.PI <= theta2;

          if (command.disabled === true || command.enabled === false) {
            inThisCommand = false;
          }

          if (isMouseCircleIn && inThisCommand) {
            activeCommandI = i;
            break;
          }

          theta1 += dtheta;
          theta2 += dtheta;
        }

        queueDrawCommands(rx, ry, r, theta);
      })

      .on('tapdrag', dragHandler)

      .on('cxttapend tapend', () => {
        parent.style.display = 'none';

        if (activeCommandI !== undefined) {
          const { select } = commands[activeCommandI];

          if (select) {
            select.apply(target, [target, gestureStartEvent]);
            activeCommandI = undefined;
          }
        }

        inGesture = false;

        restoreGestures();
      });
  }

  function removeEventListeners() {
    const { handlers } = data;

    for (let i = 0; i < handlers.length; i++) {
      const h = handlers[i];

      if (h.selector === 'core') {
        cy.off(h.events, h.fn);
      } else {
        cy.off(h.events, h.selector, h.fn);
      }
    }

    window.removeEventListener('resize', updatePixelRatio);
  }

  function destroyInstance() {
    redrawing = false;

    removeEventListeners();

    wrapper.remove();
  }

  addEventListeners();

  return {
    destroy() {
      destroyInstance();
    },
  };
};

module.exports = cxtmenu;
