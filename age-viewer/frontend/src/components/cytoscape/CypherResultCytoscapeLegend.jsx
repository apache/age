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

import React, { Component } from 'react';
import PropTypes from 'prop-types';
import { Badge } from 'react-bootstrap';
import uuid from 'react-uuid';
import { FontAwesomeIcon } from '@fortawesome/react-fontawesome';
import { faAngleDown, faAngleUp } from '@fortawesome/free-solid-svg-icons';

class CypherResultCytoscapeLegend extends Component {
  constructor(props) {
    super(props);
    this.state = {
      nodeBadges: new Map(),
      edgeBadges: new Map(),
      nodeLegendExpanded: false,
      edgeLegendExpanded: false,
      legendData: props.legendData,
    };
  }

  componentDidMount() {
  }

  static getDerivedStateFromProps(nextProps, prevState) {
    let newNodeBadges = prevState.nodeBadges;
    let newEdgeBadges = prevState.edgeBadges;
    if (nextProps.isReloading) {
      newNodeBadges = new Map();
      newEdgeBadges = new Map();
    }

    for (let i1 = 0; i1 < Object.entries(nextProps.legendData.nodeLegend).length; i1 += 1) {
      const [label, legend] = Object.entries(nextProps.legendData.nodeLegend)[i1];
      if (prevState.legendData !== undefined && prevState.legendData.nodeLegend !== undefined) {
        let isChanged = false;
        for (let i = 0; i < Object.entries(prevState.legendData.nodeLegend).length; i += 1) {
          const [prevLabel, prevLegend] = Object.entries(prevState.legendData.nodeLegend)[i];
          if (label === prevLabel && legend.color !== prevLegend.color) {
            isChanged = true;
          } else if (label === prevLabel && legend.size !== prevLegend.size) {
            isChanged = true;
          } else if (label === prevLabel && legend.caption !== prevLegend.caption) {
            isChanged = true;
          }
        }

        if (isChanged) {
          nextProps.onLabelClick({
            type: 'labels',
            data: {
              type: 'node',
              backgroundColor: legend.color,
              fontColor: legend.fontColor,
              size: legend.size,
              label,
            },
          });
        }
      }

      newNodeBadges.set(label,
        <Badge
          className="nodeLabel px-3 py-2 mx-1 my-2"
          pill
          key={uuid()}
          onClick={() => nextProps.onLabelClick({
            type: 'labels',
            data: {
              type: 'node',
              backgroundColor: legend.color,
              fontColor: legend.fontColor,
              size: legend.size,
              label,
            },
          })}
          style={{ backgroundColor: legend.color, color: legend.fontColor }}
        >
          {label}
        </Badge>);
    }

    for (let i = 0; i < Object.entries(nextProps.legendData.edgeLegend).length; i += 1) {
      const [label, legend] = Object.entries(nextProps.legendData.edgeLegend)[i];
      if (prevState.legendData !== undefined && prevState.legendData.edgeLegend !== undefined) {
        let isChanged = false;
        for (let i1 = 0; i1 < Object.entries(prevState.legendData.edgeLegend).length; i1 += 1) {
          const [prevLabel, prevLegend] = Object.entries(prevState.legendData.edgeLegend)[i1];
          if (label === prevLabel && legend.color !== prevLegend.color) {
            isChanged = true;
          } else if (label === prevLabel && legend.size !== prevLegend.size) {
            isChanged = true;
          } else if (label === prevLabel && legend.caption !== prevLegend.caption) {
            isChanged = true;
          }
        }

        if (isChanged) {
          nextProps.onLabelClick({
            type: 'labels',
            data: {
              type: 'edge',
              backgroundColor: legend.color,
              fontColor: legend.fontColor,
              size: legend.size,
              label,
            },
          });
        }
      }
      newEdgeBadges.set(label,
        <Badge
          className="edgeLabel px-3 py-2 mx-1 my-2"
          key={uuid()}
          onClick={() => nextProps.onLabelClick({
            type: 'labels',
            data: {
              type: 'edge',
              backgroundColor: legend.color,
              fontColor: legend.fontColor,
              size: legend.size,
              label,
            },
          })}
          style={{ backgroundColor: legend.color, color: legend.fontColor }}
        >
          {label}
        </Badge>);
    }

    return {
      nodeBadges: newNodeBadges,
      edgeBadges: newEdgeBadges,
      legendData: nextProps.legendData,
    };
  }

  shouldComponentUpdate() {
    return true;
  }

  componentDidUpdate() {
  }

  render() {
    const nodeLedgend = [];
    const edgeLedgend = [];

    const {
      nodeBadges, edgeBadges, nodeLegendExpanded, edgeLegendExpanded,
    } = this.state;

    nodeBadges.forEach((value) => nodeLedgend.push(value));
    edgeBadges.forEach((value) => edgeLedgend.push(value));

    return (
      <div className="legend-area">
        <div className="d-flex nodeLegend">
          <div className={`mr-auto legends legend ${nodeLegendExpanded ? 'expandedLegend' : ''}`}>
            <span>Node: </span>
            {nodeLedgend}
          </div>
          <button
            type="button"
            className="frame-head-button btn btn-link px-3"
            onClick={() => this.setState({ nodeLegendExpanded: !nodeLegendExpanded })}
          >
            <FontAwesomeIcon
              icon={nodeLegendExpanded ? faAngleUp : faAngleDown}
            />
          </button>
        </div>
        <div className="d-flex edgeLegend">
          <div className={`mr-auto legends legend ${edgeLegendExpanded ? 'expandedLegend' : ''}`}>
            <span>Edge: </span>
            {edgeLedgend}
          </div>
          <button
            type="button"
            className="frame-head-button btn btn-link px-3"
            onClick={() => this.setState({ edgeLegendExpanded: !edgeLegendExpanded })}
          >
            <FontAwesomeIcon
              icon={edgeLegendExpanded ? faAngleUp : faAngleDown}
            />
          </button>
        </div>

      </div>
    );
  }
}

CypherResultCytoscapeLegend.propTypes = {
  legendData: PropTypes.shape({
    // eslint-disable-next-line react/forbid-prop-types
    nodeLegend: PropTypes.any,
    // eslint-disable-next-line react/forbid-prop-types
    edgeLegend: PropTypes.any,
  }).isRequired,
  isReloading: PropTypes.bool.isRequired,
  onLabelClick: PropTypes.func.isRequired,
};

export default CypherResultCytoscapeLegend;
