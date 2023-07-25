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
import { createAsyncThunk, createSlice } from '@reduxjs/toolkit';

// eslint-disable-next-line no-unused-vars
const validateSamePathVariableReturn = (cypherQuery) => {
  const cypherPathValidator = new RegExp('^match\\s([a-zA-Z0-9].*)\\s*=\\s*\\(', 'i');

  if (cypherPathValidator.test(cypherQuery)) {
    const pathAlias = RegExp.$1;
    const returnPathAliasValidator = new RegExp(`^match\\s.*\\s*=.*return\\s${pathAlias}.*`, 'i');
    if (!returnPathAliasValidator.test(cypherQuery)) {
      throw Object.assign(new Error(`Only Path variable should be returned.\n Modify the return clause to ' RETURN ${pathAlias} '`), { code: 500 });
    }
  }
};

// eslint-disable-next-line no-unused-vars
const validateVlePathVariableReturn = (cypherQuery) => {
  const cypherVleValidator = new RegExp('^match\\s.*[.*[0-9]*\\s*\\.\\.\\s*[0-9]*]', 'i');

  if (cypherVleValidator.test(cypherQuery)) {
    const cypherPathValidator = new RegExp('^match\\s(.*[a-zA-Z0-9])\\s*=', 'i');

    if (!cypherPathValidator.test(cypherQuery)) {
      throw Object.assign(new Error("Path variable is required to be used with VLE query. Refer the below proper cypher query with VLE. \n 'MATCH pathvariable = (v)-[r*1..5]->(v2) return pathvariable;"), { code: 500 });
    }
  }
};

export const executeCypherQuery = createAsyncThunk(
  'cypher/executeCypherQuery',
  async (args, thunkAPI) => {
    try {
      const response = await fetch('/api/v1/cypher',
        {
          method: 'POST',
          headers: {
            Accept: 'application/json',
            'Content-Type': 'application/json',
          },
          body: JSON.stringify({ cmd: args[1] }),
          signal: thunkAPI.signal,
        });
      if (response.ok) {
        const res = await response.json();
        return { key: args[0], query: args[1], ...res };
      }
      throw response;
    } catch (error) {
      if (error.json === undefined) {
        throw error;
      } else {
        const errorJson = await error.json();
        const messaage = errorJson.message.charAt(0).toUpperCase() + errorJson.message.slice(1);
        throw messaage;
      }
    }
  },

);

const removeActive = (state, key) => {
  state.activeRequests = state.activeRequests.filter((ref) => ref !== key);
};

const CypherSlice = createSlice({
  name: 'cypher',
  initialState: {
    queryResult: {},
    activeRequests: [],
    labels: { nodeLabels: {}, edgeLabels: {} },
  },
  reducers: {
    setLabels: {
      reducer: (state, action) => {
        if (action.payload.elementType === 'node') {
          if (state.labels.nodeLabels[action.payload.label] === undefined) {
            state.labels.nodeLabels[action.payload.label] = action.payload.property;
          } else {
            Object.assign(state.labels.nodeLabels[action.payload.label], action.payload.property);
          }
        } else if (action.payload.elementType === 'edge') {
          if (state.labels.edgeLabels[action.payload.label] === undefined) {
            state.labels.edgeLabels[action.payload.label] = action.payload.property;
          } else {
            Object.assign(state.labels.edgeLabels[action.payload.label], action.payload.property);
          }
        }
      },
      prepare: (elementType, label, property) => ({ payload: { elementType, label, property } }),
    },
    removeActiveRequests: (state, action) => removeActive(state, action.payload),
  },
  extraReducers: {
    [executeCypherQuery.fulfilled]: (state, action) => {
      Object.assign(state.queryResult[action.payload.key], {
        ...action.payload,
        complete: true,
      });
      removeActive(state, action.payload.key);
    },
    [executeCypherQuery.pending]: (state, action) => {
      const key = action.meta.arg[0];
      const command = action.meta.arg[1];
      const rid = action.meta.requestId;
      state.queryResult[key] = {};
      state.activeRequests = [...state.activeRequests, key];
      Object.assign(state.queryResult[key], {
        command,
        complete: false,
        requestId: rid,
      });
    },
    [executeCypherQuery.rejected]: (state, action) => {
      removeActive(state, action.meta.arg[0]);
      state.queryResult[action.meta.arg[0]] = {
        command: 'ERROR',
        query: action.meta.arg[1],
        key: action.meta.arg[0],
        complete: true,
        message: action.error.message,
      };
    },
  },
});

export const { setLabels, removeActiveRequests } = CypherSlice.actions;

export default CypherSlice.reducer;
