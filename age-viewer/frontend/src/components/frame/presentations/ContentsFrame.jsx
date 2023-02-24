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

import PropTypes from 'prop-types';
import React, { useEffect, useState } from 'react';
import { useDispatch } from 'react-redux';
import Frame from '../Frame';
import FrameStyles from '../Frame.module.scss';
import { removeFrame } from '../../../features/frame/FrameSlice';
import { addAlert } from '../../../features/alert/AlertSlice';

const ContentFrame = ({
  refKey,
  isPinned,
  reqString,
  playTarget,
}) => {
  const dispatch = useDispatch();
  const [slides] = useState([]);
  const [currentSlide, setCurrentSlide] = useState(0);

  useEffect(() => {
    dispatch(addAlert('ErrorPlayLoadFail', playTarget));
    dispatch(removeFrame(refKey));
  }, []);

  const genCarousel = () => {
    const slideSize = slides.length;
    const carousel = [];
    for (let i = 0; i < slideSize; i += 1) {
      carousel.push(
        <div
          aria-hidden="true"
          onClick={() => setCurrentSlide(i)}
          style={{
            width: '30px',
            height: '3px',
            cursor: 'pointer',
            background: currentSlide === i ? 'rgba(0,0,0,.13)' : '#6c757d',
            margin: '5px',
            border: 0,
            opacity: '0.5',
          }}
        />,
      );
    }
    return carousel;
  };

  return (
    <Frame
      reqString={reqString}
      isPinned={isPinned}
      bodyNoPadding
      refKey={refKey}
    >
      <div
        className={`${FrameStyles.FlexContentWrapper} ${FrameStyles.DefaultLimitWrapper}`}
        id={refKey}
        style={{ padding: 'initial' }}
      >
        <div style={{
          flex: 1,
          padding: '15px 30px',
          overflowY: 'auto',
        }}
        >
          {slides[currentSlide]}
        </div>
        <div style={{
          width: '100%',
          display: 'flex',
          justifyContent: 'center',
          padding: '1rem',
        }}
        >
          {genCarousel()}
        </div>
      </div>
    </Frame>
  );
};

ContentFrame.propTypes = {
  refKey: PropTypes.string.isRequired,
  isPinned: PropTypes.bool.isRequired,
  reqString: PropTypes.string.isRequired,
  playTarget: PropTypes.string.isRequired,
};

export default ContentFrame;
