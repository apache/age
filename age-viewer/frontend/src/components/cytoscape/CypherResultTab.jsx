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

/* eslint-disable react/react-in-jsx-scope */
import { Component } from 'react';
import PropTypes from 'prop-types';
import { FontAwesomeIcon } from '@fortawesome/react-fontawesome';
import { faTable } from '@fortawesome/free-solid-svg-icons';
import IconGraph from '../../icons/IconGraph';

class CypherResultTab extends Component {
  constructor(props) {
    super(props);
    this.state = {};
    this.refKey = props.refKey;
    this.currentTab = props.currentTab;
    this.setIsTable = props.setIsTable;
  }

  render() {
    const activeTab = (refKey, tabType) => {
      if (tabType === 'graph') {
        document.getElementById(`${refKey}-${tabType}`).classList.add('selected-frame-tab');
        document.getElementById(`${refKey}-${tabType}`).classList.remove('deselected-frame-tab');
        document.getElementById(`${refKey}-table`).classList.add('deselected-frame-tab');
        document.getElementById(`${refKey}-table`).classList.remove('selected-frame-tab');
      } else if (tabType === 'table') {
        document.getElementById(`${refKey}-${tabType}`).classList.add('selected-frame-tab');
        document.getElementById(`${refKey}-${tabType}`).classList.remove('deselected-frame-tab');
        document.getElementById(`${refKey}-graph`).classList.add('deselected-frame-tab');
        document.getElementById(`${refKey}-graph`).classList.remove('selected-frame-tab');
      }
    };
    return (
      <div className="legend-button-area">
        <button
          className="btn"
          type="button"
          style={{ width: '50%', fontSize: '14px', color: this.currentTab === 'graph' ? '#142B80' : '#495057' }}
          onClick={() => { activeTab(this.refKey, 'graph'); this.setIsTable(false); }}
        >
          <IconGraph />
          <br />
          <b style={{ fontSize: '14px;' }}>Graph</b>
        </button>
        <div
          style={{
            backgroundColor: '#C4C4C4',
            width: '1px',
            height: '76px',
            marginTop: '20px',
          }}
        />
        <button
          className="btn"
          type="button"
          style={{ width: '50%', fontSize: '14px', color: this.currentTab === 'table' ? '#142B80' : '#495057' }}
          onClick={() => { activeTab(this.refKey, 'table'); this.setIsTable(true); }}
        >
          <FontAwesomeIcon icon={faTable} style={{ fontSize: '25px' }} />
          <br />
          <b style={{ fontSize: '14px;' }}>Table</b>
        </button>
      </div>
    );
  }
}

CypherResultTab.propTypes = {
  refKey: PropTypes.string.isRequired,
  currentTab: PropTypes.string.isRequired,
  setIsTable: PropTypes.func.isRequired,
};

export default CypherResultTab;
