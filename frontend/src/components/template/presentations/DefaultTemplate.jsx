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

import React, { useEffect, useState } from 'react';
import PropTypes from 'prop-types';
import { useDispatch } from 'react-redux';
import { Row, Button } from 'react-bootstrap';
import { FontAwesomeIcon } from '@fortawesome/react-fontawesome';
import { faBars } from '@fortawesome/free-solid-svg-icons';
import EditorContainer from '../../contents/containers/Editor';
import Sidebar from '../../sidebar/containers/Sidebar';
import Contents from '../../contents/containers/Contents';
import Modal from '../../modal/containers/Modal';
import { loadFromCookie, saveToCookie } from '../../../features/cookie/CookieUtil';
import BuilderContainer from '../../query_builder/BuilderContainer';
import './DefaultTemplate.scss';
import KeyWordFinder from '../../../features/query_builder/KeyWordFinder';

const DefaultTemplate = ({
  theme,
  maxNumOfFrames,
  maxNumOfHistories,
  maxDataOfGraph,
  maxDataOfTable,
  changeSettings,
  isOpen,
}) => {
  const dispatch = useDispatch();
  const [open, setOpen] = useState(false);
  const [stateValues] = useState({
    theme,
    maxNumOfFrames,
    maxNumOfHistories,
    maxDataOfGraph,
    maxDataOfTable,
  });
  const [finder, setFinder] = useState(null);

  useEffect(async () => {
    const req = {
      method: 'GET',
    };
    const res = await fetch('/api/v1/miscellaneous', req);
    const results = await res.json();
    const kwFinder = KeyWordFinder.fromMatrix(results);
    setFinder(kwFinder);
  }, []);

  useEffect(() => {
    let isChanged = false;
    const cookieState = {
      theme,
      maxNumOfFrames,
      maxNumOfHistories,
      maxDataOfGraph,
      maxDataOfTable,
    };

    Object.keys(stateValues).forEach((key) => {
      let fromCookieValue = loadFromCookie(key);

      if (fromCookieValue !== undefined && key !== 'theme') {
        fromCookieValue = parseInt(fromCookieValue, 10);
      }

      if (fromCookieValue === undefined) {
        saveToCookie(key, stateValues[key]);
      } else if (fromCookieValue !== stateValues[key]) {
        cookieState[key] = fromCookieValue;
        isChanged = true;
      }
    });

    if (isChanged) {
      dispatch(() => changeSettings(Object.assign(stateValues, cookieState)));
    }
  });

  return (
    <div className="default-template">
      { isOpen && <Modal /> }
      <input
        type="radio"
        className="theme-switch"
        name="theme-switch"
        id="default-theme"
        checked={theme === 'default'}
        readOnly
      />
      <input
        type="radio"
        className="theme-switch"
        name="theme-switch"
        id="dark-theme"
        checked={theme === 'dark'}
        readOnly
      />
      <Row className="content-row">
        <div>
          <Button onClick={() => setOpen(true)}>
            <FontAwesomeIcon icon={faBars} />
          </Button>
          <BuilderContainer open={open} setOpen={setOpen} finder={finder} />
        </div>
        <div className="editor-division wrapper-extension-padding">

          <EditorContainer />
          <Sidebar />
          <Contents />

        </div>

      </Row>

    </div>
  );
};

DefaultTemplate.propTypes = {
  theme: PropTypes.string.isRequired,
  maxNumOfFrames: PropTypes.number.isRequired,
  maxNumOfHistories: PropTypes.number.isRequired,
  maxDataOfGraph: PropTypes.number.isRequired,
  maxDataOfTable: PropTypes.number.isRequired,
  changeSettings: PropTypes.func.isRequired,
  isOpen: PropTypes.bool.isRequired,
};

export default DefaultTemplate;
