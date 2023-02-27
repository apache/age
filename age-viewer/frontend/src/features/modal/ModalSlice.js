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

/* eslint-disable no-param-reassign */
import { createSlice } from '@reduxjs/toolkit';

const ModalSlice = createSlice({
  name: 'modal',
  initialState: {
    isOpen: false,
    isTutorial: false,
    graphHistory: [],
    elementHistory: [],
  },
  reducers: {
    openModal: {
      reducer: (state) => {
        state.isOpen = true;
      },
    },
    closeModal: {
      reducer: (state) => {
        state.isOpen = false;
      },
    },
    openTutorial: {
      reducer: (state) => {
        state.isTutorial = true;
      },
    },
    closeTutorial: {
      reducer: (state) => {
        state.isTutorial = false;
      },
    },
    addGraphHistory: {
      reducer: (state, action) => {
        state.graphHistory.push(action.payload.graph);
      },
      prepare: (graph) => ({ payload: { graph } }),
    },
    addElementHistory: {
      reducer: (state, action) => {
        state.elementHistory.push(action.payload.element);
      },
      prepare: (element) => ({ payload: { element } }),
    },
    removeGraphHistory: {
      reducer: (state) => {
        state.graphHistory = [];
      },
    },
    removeElementHistory: {
      reducer: (state) => {
        state.elementHistory = [];
      },
    },
  },
});

export const {
  openModal,
  closeModal,
  openTutorial,
  closeTutorial,
  addGraphHistory,
  addElementHistory,
  removeGraphHistory,
  removeElementHistory,
} = ModalSlice.actions;

export default ModalSlice.reducer;
