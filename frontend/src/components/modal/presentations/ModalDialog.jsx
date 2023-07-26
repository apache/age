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
import { useDispatch } from 'react-redux';
import { Button, Modal } from 'react-bootstrap';

const ModalDialog = ({
  closeModal,
  graphHistory,
  elementHistory,
  removeGraphHistory,
  removeElementHistory,
  getMetaData,
  currentGraph,
}) => {
  const dispatch = useDispatch();

  const removeNode = () => {
    fetch('/api/v1/cypher',
      {
        method: 'POST',
        headers: {
          Accept: 'application/json',
          'Content-Type': 'application/json',
        },
        body: JSON.stringify({ cmd: `SELECT * FROM cypher('${graphHistory[0]}', $$ MATCH (S) WHERE id(S) = ${elementHistory[0]} DETACH DELETE S $$) as (S agtype);` }),
      })
      .then((res) => {
        if (res.ok) {
          dispatch(removeGraphHistory());
          dispatch(removeElementHistory());
          dispatch(closeModal());
          alert('The node has been deleted from your database. Please re-run the query.');
          getMetaData({ currentGraph });
        }
      });
  };

  return (
    <div className="modal-container">
      <div
        style={{ display: 'block', position: 'initial' }}
      >
        <Modal.Dialog>
          <Modal.Header closeButton onClick={() => { dispatch(closeModal()); }}>
            <Modal.Title>Delete Confirmation</Modal.Title>
          </Modal.Header>

          <Modal.Body>
            <p>
              After clicking on confirm,
              the node and related edge will be deleted from the database.
            </p>
          </Modal.Body>

          <Modal.Footer>
            <Button variant="secondary" onClick={() => { dispatch(closeModal()); }}>Cancel</Button>
            <Button variant="primary" onClick={() => { removeNode(); }}>Confirm</Button>
          </Modal.Footer>
        </Modal.Dialog>
      </div>
    </div>
  );
};

ModalDialog.propTypes = {
  closeModal: PropTypes.func.isRequired,
  // eslint-disable-next-line react/forbid-prop-types
  graphHistory: PropTypes.any.isRequired,
  // eslint-disable-next-line react/forbid-prop-types
  elementHistory: PropTypes.any.isRequired,
  removeGraphHistory: PropTypes.func.isRequired,
  removeElementHistory: PropTypes.func.isRequired,
  getMetaData: PropTypes.func.isRequired,
  currentGraph: PropTypes.string.isRequired,
};

export default ModalDialog;
