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
const removeEles = function (query, ancestor = document) {
  const els = ancestor.querySelectorAll(query);

  for (let i = 0; i < els.length; i++) {
    const el = els[i];

    el.parentNode.removeChild(el);
  }
};

const setStyles = function (el, style) {
  const props = Object.keys(style);

  for (let i = 0, l = props.length; i < l; i++) {
    el.style[props[i]] = style[props[i]];
  }
};

const createElement = function (options) {
  options = options || {};

  const el = document.createElement(options.tag || 'div');

  el.className = options.class || '';

  if (options.style) {
    setStyles(el, options.style);
  }

  return el;
};

const getPixelRatio = function () {
  return window.devicePixelRatio || 1;
};

const getOffset = function (el) {
  const offset = el.getBoundingClientRect();

  return {
    left: offset.left + document.body.scrollLeft
          + parseFloat(getComputedStyle(document.body)['padding-left'])
          + parseFloat(getComputedStyle(document.body)['border-left-width']),
    top: offset.top + document.body.scrollTop
         + parseFloat(getComputedStyle(document.body)['padding-top'])
         + parseFloat(getComputedStyle(document.body)['border-top-width']),
  };
};

module.exports = {
  removeEles, setStyles, createElement, getPixelRatio, getOffset,
};
