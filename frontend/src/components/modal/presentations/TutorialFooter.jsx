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
import { Modal, Button } from 'react-bootstrap';

const TutorialFooter = ({ page, setPage, closeTutorial }) => {
  const [curPage, setCurPage] = useState();

  useEffect(() => {
    setCurPage(page);
  }, [page]);

  return (
    <Modal.Footer>
      <div>
        <Button onClick={() => closeTutorial()} className="tutorial-button" variant="secondary">Close</Button>
      </div>
      <div>
        <Button className="tutorial-button" variant={curPage === 1 ? 'outline-secondary' : 'secondary'} style={{ marginRight: '1rem' }} onClick={() => { setPage(curPage > 1 ? curPage - 1 : curPage); }}>Previous Tip</Button>
        <Button className="tutorial-button" variant={curPage === 5 ? 'outline-primary' : 'primary'} onClick={() => { setPage(curPage < 5 ? curPage + 1 : curPage); }}>Next Tip</Button>
      </div>
    </Modal.Footer>
  );
};

TutorialFooter.propTypes = {
  page: PropTypes.number.isRequired,
  setPage: PropTypes.func.isRequired,
  closeTutorial: PropTypes.func.isRequired,
};

export default TutorialFooter;
