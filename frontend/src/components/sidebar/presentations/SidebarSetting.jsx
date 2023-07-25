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
import PropTypes from 'prop-types';
import { ColoredLine, SubLabelLeft } from './SidebarComponents';
import { saveToCookie } from '../../../features/cookie/CookieUtil';

const SidebarSetting = ({
  theme,
  maxNumOfFrames,
  maxNumOfHistories,
  maxDataOfGraph,
  maxDataOfTable,
  changeTheme,
  changeMaxNumOfFrames,
  changeMaxNumOfHistories,
  changeMaxDataOfGraph,
  changeMaxDataOfTable,
  resetSetting,
}) => (
  <div className="sidebar-setting">
    <div className="sidebar sidebar-header">
      <h4>Configuration</h4>
    </div>
    <div className="sidebar sidebar-body">
      <div className="form-group">
        <b>Themes</b>
        <ColoredLine />
        <select
          className="form-control theme-switcher"
          value={theme}
          onChange={(e) => [saveToCookie('theme', e.target.value), changeTheme(e)]}
        >
          <option value="default">Default</option>
          <option value="dark">Dark</option>
        </select>
      </div>
      <div className="form-group pt-4">
        <b>Frames</b>
        <ColoredLine />
        <fieldset className="form-group">
          <SubLabelLeft label="Maximum Number of Frames:" classes="py-1" />
          <input
            type="number"
            className="form-control"
            id="maxFrames"
            name="maxFrames"
            min="0"
            value={maxNumOfFrames}
            onChange={(e) => [saveToCookie('maxNumOfFrames', e.target.value), changeMaxNumOfFrames(parseInt(e.target.value, 10))]}
          />
        </fieldset>
        <fieldset className="form-group">
          <SubLabelLeft label="Max Number of Histories:" classes="py-1" />
          <input
            type="number"
            className="form-control"
            id="maxHistories"
            name="maxHistories"
            value={maxNumOfHistories}
            min="0"
            onChange={(e) => [saveToCookie('maxNumOfHistories', e.target.value), changeMaxNumOfHistories(parseInt(e.target.value, 10))]}
          />
        </fieldset>
      </div>
      <div className="form-group pt-4">
        <b>Data Display</b>
        <ColoredLine />
        <fieldset className="form-group">
          <SubLabelLeft label="Maximum Data of Graph Visualization" classes="py-1" />
          <input
            type="number"
            className="form-control"
            id="maxGraphData"
            name="maxGraphData"
            value={maxDataOfGraph.toString()}
            min="0"
            onChange={(e) => [saveToCookie('maxDataOfGraph', e.target.value), changeMaxDataOfGraph(parseInt(e.target.value, 10))]}
          />
        </fieldset>
        <fieldset className="form-group">
          <SubLabelLeft label="Maximum Data of Table Display" classes="py-1" />
          <input
            type="number"
            className="form-control"
            id="maxTableData"
            name="maxTableData"
            value={maxDataOfTable}
            min="0"
            onChange={(e) => [saveToCookie('maxDataOfTable', e.target.value), changeMaxDataOfTable(parseInt(e.target.value, 10))]}
          />
        </fieldset>
      </div>
      <div className="form-group pt-4">
        <fieldset className="form-group">
          <button
            type="button"
            className="btn btn-info btn-sm btn-block"
            onClick={() => [
              resetSetting(),
            ]}
          >
            Reset Configuration
          </button>
        </fieldset>
      </div>
    </div>
  </div>
);

SidebarSetting.propTypes = {
  theme: PropTypes.string.isRequired,
  maxNumOfFrames: PropTypes.number.isRequired,
  maxNumOfHistories: PropTypes.number.isRequired,
  maxDataOfGraph: PropTypes.number.isRequired,
  maxDataOfTable: PropTypes.number.isRequired,
  changeTheme: PropTypes.func.isRequired,
  changeMaxNumOfFrames: PropTypes.func.isRequired,
  changeMaxNumOfHistories: PropTypes.func.isRequired,
  changeMaxDataOfGraph: PropTypes.func.isRequired,
  changeMaxDataOfTable: PropTypes.func.isRequired,
  resetSetting: PropTypes.func.isRequired,
};

export default SidebarSetting;
