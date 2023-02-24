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

import React, { useState, useEffect } from 'react';
import PropTypes from 'prop-types';
import { Modal } from 'react-bootstrap';

const TutorialHeader = ({ page }) => {
  const [curPage, setCurPage] = useState();

  useEffect(() => {
    setCurPage(page);
  }, [page]);

  return (
    <Modal.Header
      style={{
        padding: '0.3rem 0.5rem 0 0.5rem', borderBottom: '1px solid black', margin: '0', background: '#A9A9A9',
      }}
    >
      <Modal.Title style={{ fontSize: '0.88rem', paddingBottom: '0px', color: '#F0FFF0' }}>
        {`Tip of AGE Viewer -${curPage}`}
      </Modal.Title>
    </Modal.Header>
  );
};

TutorialHeader.propTypes = {
  page: PropTypes.string.isRequired,
};

export default TutorialHeader;
