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

class KeyWordFinder {
  constructor() {
    this.keywordMap = new Map();
    this.allKeywords = new Set();
  }

  getConnectedNames(kw) {
    const key = kw.toUpperCase();
    if (!this.allKeywords.has(key)) {
      return KeyWordFinder.INITIAL;
    }
    const relationships = this.keywordMap[key];
    const keywordList = Object.keys(this.keywordMap);
    const relatedKeys = [];
    relationships.forEach((element, index) => {
      if (element !== '0') {
        relatedKeys.push(keywordList[index]);
      }
    });
    return relatedKeys;
  }

  hasWord(word) {
    const upperWord = word.toUpperCase();
    return this.allKeywords.has(upperWord);
  }

  static get INITIAL() {
    return ['MATCH', 'CREATE', 'MERGE'];
  }

  static fromMatrix(data) {
    const { kw, relationships } = data;
    const finder = new KeyWordFinder();
    // kw is list of keywordList and relationships is matrix
    kw.forEach((element, index) => {
      if (element === '') return;
      finder.keywordMap[element] = relationships[index].slice(1);
      finder.allKeywords.add(element);
    });
    return finder;
  }
}

export default KeyWordFinder;
