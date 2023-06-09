import React from 'react';
import { Link } from 'react-router-dom';
import './styles.css';

const FirstPage = () => {
  return (
    <div className="container">
      <div>
        <h1 className="container__heading">
          <b>Welcome to Apache Age Viewer</b>
        </h1>
      </div>
      <div id="logo" className="picture">
        <img
          src="https://www.apache.org/logos/res/age/age.png"
          alt="logo"
          style={{ width: '80%', height: '50%' }}
        />
      </div>

      <Link to="/mainpage">
        <button class="popup__button">Start</button>
      </Link>
    </div>
  );
};

export default FirstPage;
