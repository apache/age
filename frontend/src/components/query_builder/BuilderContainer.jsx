import {
  Button, Drawer, Select, Space,
} from 'antd';
import React, { useState, useEffect } from 'react';
import { useDispatch, useSelector } from 'react-redux';
import PropTypes from 'prop-types';
import CodeMirror from '../editor/containers/CodeMirrorWapperContainer';
import BuilderSelection from './BuilderSelection';
import KeyWordFinder from '../../features/query_builder/KeyWordFinder';

import { setCommand } from '../../features/editor/EditorSlice';
import './BuilderContainer.scss';

const BuilderContainer = ({ open, setOpen, finder }) => {
  const [query, setQuery] = useState('');
  const [currentWord, setCurrentWord] = useState('');
  const [selectedGraph, setSelectedGraph] = useState('');
  const [availableGraphs, setAvailableGraphs] = useState([]);
  const metadata = useSelector((state) => state.metadata);
  const dispatch = useDispatch();
  useEffect(() => {
    setAvailableGraphs(Object.keys(metadata.graphs));
  }, [metadata]);

  const getCurrentWord = (q) => {
    const words = q.split(/[ ,\n]/);
    const isWord = words.findLast((element) => finder.hasWord(element));
    const word = isWord || '';
    setCurrentWord(word);
  };

  const handleSetQuery = (word) => {
    const fullQuery = query !== '' ? `${query.trim()}\n${word}` : word;

    setQuery(fullQuery);
    getCurrentWord(fullQuery);
  };
  const handleSelectGraph = (s) => {
    setSelectedGraph(s);
  };

  const handleSubmit = () => {
    const finalQuery = `SELECT * FROM cypher('${selectedGraph}', $$ ${query} $$) as (V agtype)`;
    dispatch((setCommand(finalQuery)));
    setOpen(false);
  };
  return (
    <Drawer
      title="Query Generator"
      open={open}
      onClose={() => setOpen(!open)}
      placement="left"
    >
      <Select
        id="graph-selection"
        onChange={handleSelectGraph}
        placeholder="Select Graph"
        value={selectedGraph}
      >
        {
          availableGraphs.map((s) => (
            <Select.Option
              value={s}
            />
          ))
          }
      </Select>

      <Space />
      <div className="code-mirror-builder">
        <CodeMirror onChange={handleSetQuery} value={query} />
      </div>

      <Space />
      <div className="selection-builder">
        <BuilderSelection
          finder={finder}
          setQuery={handleSetQuery}
          currentWord={currentWord}
        />
      </div>
      <div id="submit-builder">
        <Button size="sm" onClick={handleSubmit}>Submit</Button>
      </div>

    </Drawer>
  );
};
BuilderContainer.propTypes = {
  open: PropTypes.bool.isRequired,
  setOpen: PropTypes.func.isRequired,
  finder: PropTypes.shape(KeyWordFinder).isRequired,
};
export default BuilderContainer;
