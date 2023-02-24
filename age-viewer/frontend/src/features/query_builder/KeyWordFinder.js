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
