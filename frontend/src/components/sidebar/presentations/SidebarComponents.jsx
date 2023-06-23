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

import React from 'react';
import { Select } from 'antd';
import { Col } from 'react-bootstrap';
import PropTypes from 'prop-types';
import './Components.scss';

const StyleTextRight = {
  marginBottom: '10px', textAlign: 'right', fontSize: '13px', fontWeight: 'bold',
};
const StyleTextLeft = { fontSize: '13px', fontWeight: 'bold' };

export const ColoredLine = () => (
  <hr
    style={{
      color: '#B0B0B0',
      backgroundColor: '#B0B0B0',
      marginTop: 0,
      height: 0.3,
    }}
  />
);

export const VerticalLine = () => (
  <div
    style={{
      backgroundColor: '#C4C4C4',
      width: '1px',
      height: '120px',
      marginTop: '37px',
      marginBottom: '37px',
    }}
  />
);

export const HorizontalLine = () => (
  <div
    className="horizontalLine"
    style={{
      border: '1px solid #C4C4C4',
      opacity: '1',
      width: '80%',
      height: '0',
      margin: '3px auto',
    }}
  />
);

const SubLabelRight = ({ label, classes }) => (
  <div className={classes} style={StyleTextRight}>{label}</div>
);
SubLabelRight.propTypes = {
  classes: PropTypes.string.isRequired,
  label: PropTypes.string.isRequired,
};

const SubLabelLeft = ({ label, classes }) => (
  <div className={classes} style={StyleTextLeft}>{label}</div>
);
SubLabelLeft.propTypes = {
  classes: PropTypes.string.isRequired,
  label: PropTypes.oneOfType([
    PropTypes.string,
    PropTypes.element,
  ]).isRequired,
};

const SubLabelLeftWithLink = ({ label, classes }) => (
  <div className={classes} style={StyleTextLeft}><pre>{label}</pre></div>
);
SubLabelLeftWithLink.propTypes = {
  classes: PropTypes.string.isRequired,
  label: PropTypes.string.isRequired,
};

const GraphSelectDropdown = ({
  currentGraph, graphs, changeCurrentGraph, changeGraphDB,
}) => {
  const selectStyle = {
    marginTop: '1rem',
    display: 'block',
  };
  const handleGraphClick = (_, e) => {
    changeCurrentGraph({ id: e['data-gid'] });
    changeGraphDB({ graphName: e.value });
  };

  const options = (
    graphs.map(([gname, graphId]) => (
      <Select.Option value={gname} data-gid={graphId}>{gname}</Select.Option>
    ))
  );
  return (
    <Col id="graphSelectionContainer">
      <Select onChange={handleGraphClick} placeholder="Select Graph" style={selectStyle} value={currentGraph}>
        {options}
      </Select>
      <br />
      <b>
        Current Graph
      </b>
    </Col>
  );
};

GraphSelectDropdown.propTypes = {
  currentGraph: PropTypes.string.isRequired,
  graphs: PropTypes.arrayOf(PropTypes.arrayOf(PropTypes.string)).isRequired,
  changeCurrentGraph: PropTypes.func.isRequired,
  changeGraphDB: PropTypes.func.isRequired,
};

export {
  SubLabelRight, SubLabelLeft, SubLabelLeftWithLink, GraphSelectDropdown,
};
