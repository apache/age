/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

export const selectedLabel = {
  node: {},
  edge: {},
};

const getLabel = (ele, captionProp) => {
  if (captionProp === 'gid') {
    if (ele.isNode()) {
      selectedLabel.node[ele.data('label')] = 'gid';
    } else {
      selectedLabel.edge[ele.data('label')] = 'gid';
    }
    return `[ ${ele.data('id')} ]`;
  } if (captionProp === 'label') {
    if (ele.isNode()) {
      selectedLabel.node[ele.data('label')] = 'label';
    } else {
      selectedLabel.edge[ele.data('label')] = 'label';
    }
    return `[ :${ele.data('label')} ]`;
  }
  const props = ele.data('properties');
  if (props[captionProp] === undefined) {
    return '';
  }
  if (ele.isNode()) {
    selectedLabel.node[ele.data('label')] = captionProp;
  } else {
    selectedLabel.edge[ele.data('label')] = captionProp;
  }
  return props[captionProp];
};

export const stylesheet = [
  {
    selector: 'node',
    style: {
      width(ele) { return ele ? ele.data('size') : 55; },
      height(ele) { return ele ? ele.data('size') : 55; },
      label(ele) {
        const captionProp = ele.data('caption');
        return getLabel(ele, captionProp);
      },
      'background-color': function (ele) { return ele ? ele.data('backgroundColor') : '#FFF'; },
      'border-width': '3px',
      'border-color': function (ele) { return ele ? ele.data('borderColor') : '#FFF'; },
      'border-opacity': 0.6,
      'text-valign': 'center',
      'text-halign': 'center',
      color(ele) { return ele ? ele.data('fontColor') : '#FFF'; },
      'font-size': '10px',
      'text-wrap': 'ellipsis',
      'text-max-width': function (ele) { return ele ? ele.data('size') : 55; },
    },
  },
  {
    selector: 'node.highlight',
    style: {
      'border-width': '6px',
      'border-color': '#B2EBF4',
    },
  },
  {
    selector: 'node:selected',
    style: {
      'border-width': '6px',
      'border-color': '#B2EBF4',
    },
  },
  {
    selector: 'edge',
    style: {
      width(ele) { return ele ? ele.data('size') : 1; },
      label(ele) { const captionProp = ele.data('caption'); return getLabel(ele, captionProp); },
      'text-background-color': '#FFF',
      'text-background-opacity': 1,
      'text-background-padding': '3px',
      'line-color': function (ele) { return ele ? ele.data('backgroundColor') : '#FFF'; },
      'target-arrow-color': function (ele) { return ele ? ele.data('backgroundColor') : '#FFF'; },
      'target-arrow-shape': 'triangle',
      'curve-style': 'bezier',
      color(ele) { return ele ? ele.data('fontColor') : '#FFF'; },
      'font-size': '10px',
      'text-rotation': 'autorotate',
    },
  },
  {
    selector: 'edge.highlight',
    style: {
      width(ele) { return ele ? ele.data('size') : 1; },
      'line-color': '#B2EBF4',
      'target-arrow-color': '#B2EBF4',
      'target-arrow-shape': 'triangle',
      'curve-style': 'bezier',
    },
  },
  {
    selector: 'edge:selected',
    style: {
      width(ele) { return ele ? ele.data('size') : 1; },
      'line-color': '#B2EBF4',
      'target-arrow-color': '#B2EBF4',
      'target-arrow-shape': 'triangle',
      'curve-style': 'bezier',
    },
  },
];
