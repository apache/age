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
import { Modal, Image } from 'react-bootstrap';
import Agce from '../../../images/agce.gif';
import Query from '../../../images/queryEditor.png';
import Copy from '../../../images/copy.png';
import CSV from '../../../images/graphCSV.png';
import Menu from '../../../images/graphMenu.png';

const TutorialBody = ({ page, text, addiText }) => {
  const [curImg, setCurImg] = useState();
  const [addiImg, setAddiImg] = useState();
  const [link, setLink] = useState();
  useEffect(() => {
    if (page === 1) {
      setCurImg(Agce);
      setAddiImg();
    }
    if (page === 2) {
      setCurImg(Query);
      setAddiImg(Copy);
    }
    if (page === 3) {
      setLink(addiText);
      setCurImg(CSV);
      setAddiImg();
    }
    if (page === 4) setCurImg(Menu);
    if (page === 5) setCurImg();
  }, [page]);

  return (
    <Modal.Body style={{ height: '16rem', display: 'border-box', overflowY: 'scroll' }}>
      <p>
        {text}
      </p>
      <Image className="tutorial-img" src={curImg} fluid />
      <br />
      <br />
      { page === 3
        && (
          <p>
            The format of the csv files is as described here.
            <br />
            <br />
            {link}
          </p>
        )}
      { addiImg
        ? (
          <div style={{ margin: '1rem 0' }}>
            <p>
              {addiText}
            </p>
            <Image className="tutorial-img" src={addiImg} fluid />
          </div>
        ) : (<></>)}
    </Modal.Body>
  );
};

TutorialBody.propTypes = {
  page: PropTypes.number.isRequired,
  text: PropTypes.string.isRequired,
  addiText: PropTypes.string.isRequired,
};

export default TutorialBody;
