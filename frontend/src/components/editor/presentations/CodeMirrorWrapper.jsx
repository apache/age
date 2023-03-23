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

import React, { useRef, useState } from 'react';
import CodeMirror from '@uiw/react-codemirror';
import 'codemirror/keymap/sublime';
import 'codemirror/addon/display/placeholder';
import 'codemirror/theme/ambiance-mobile.css';
import './CodeMirror.scss';
import PropTypes from 'prop-types';

const CodeMirrorWrapper = ({
  value, onChange, commandHistory, onClick,
}) => {
  const [commandHistoryIndex, setCommandHistoryIndex] = useState(commandHistory.length);
  const codeMirrorRef = useRef();

  return (
    <CodeMirror
      id="editor"
      ref={codeMirrorRef}
      value={value}
      options={{
        keyMap: 'sublime',
        mode: 'cypher',
        placeholder: 'Create a query...',
        tabSize: 4,
        lineNumbers: true,
        spellcheck: false,
        autocorrect: false,
        autocapitalize: false,
        lineNumberFormatter: () => '$',
        extraKeys: {
          'Shift-Enter': (editor) => {
            onClick();
            editor.setValue('');
            setCommandHistoryIndex(-1);
          },
          'Ctrl-Enter': (editor) => {
            onClick();
            editor.setValue('');
            setCommandHistoryIndex(-1);
          },
          'Ctrl-Up': (editor) => {
            if (commandHistory.length === 0) {
              return;
            }
            if (commandHistoryIndex === -1) {
              const currentIdx = commandHistory.length - 1;
              editor.setValue(commandHistory[currentIdx]);
              setCommandHistoryIndex(currentIdx);
              return;
            }
            if (commandHistoryIndex === 0) {
              editor.setValue(commandHistory[0]);
              setCommandHistoryIndex(0);
              return;
            }

            editor.setValue(commandHistory[commandHistoryIndex - 1]);
            setCommandHistoryIndex(commandHistoryIndex - 1);
          },
          'Ctrl-Down': (editor) => {
            if (commandHistory.length === 0) {
              return;
            }
            if (commandHistoryIndex === -1) {
              editor.setValue('');
              return;
            }

            if (commandHistoryIndex === (commandHistory.length - 1)) {
              editor.setValue('');
              setCommandHistoryIndex(-1);
              return;
            }

            editor.setValue(commandHistory[commandHistoryIndex + 1]);
            setCommandHistoryIndex(commandHistoryIndex + 1);
          },
          Enter: (editor) => {
            editor.replaceSelection('\n', 'end');
          },
        },
      }}
      onChange={(editor) => {
        onChange(editor.getValue());
        const lineCount = editor.lineCount();
        let draggedHeight;
        let height;
        if (lineCount <= 1) {
          editor.setOption('lineNumberFormatter', () => '$');
        } else {
          editor.setOption('lineNumberFormatter', (number) => number);
          draggedHeight = document.getElementById('codeMirrorEditor').style.height;
          if (draggedHeight) {
            [height] = draggedHeight.split('px');
            if (height < (58 + 21 * lineCount)) {
              document.getElementById('codeMirrorEditor').style.height = null;
            }
          }
        }
        return true;
      }}
    />
  );
};

CodeMirrorWrapper.propTypes = {
  onChange: PropTypes.func.isRequired,
  value: PropTypes.string.isRequired,
  commandHistory: PropTypes.arrayOf(PropTypes.string).isRequired,
  onClick: PropTypes.func.isRequired,
};

export default CodeMirrorWrapper;
