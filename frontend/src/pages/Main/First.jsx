
import React from "react";
import { Link } from 'react-router-dom';
import MainPage from "./MainPage";

  
const First = () => {
  return (
  <div style={{textAlign:"center",backgroundColor:"rgb(53, 51, 54)"}}>
  
    <div>
      <h1 style={{textAlign:"center",color:"rgb(255,255,255)"}}><b>Welcome to Apache Age Viewer</b></h1>
    </div>
    <div id="logo">
       <img src="https://www.apache.org/logos/res/age/age.png"  alt="logo" style={{  
       display: "block",
       width: "50%",
       height: "50",
       margin: "auto",
       padding: "10% 0 0",
       backgroundPosition:"center",
       backgroundRepeat:"no-repeat",
       backgroundSize: "100% 100%",backgroundOrigin: "content-box"}} />

    </div> 
   
    <Link to="/second">
       <button class="popup-button" style={{ textAlign:"center",textDecoration: "none",fontWeight: "800",fontSize: "1em",
       textTransform: "uppercase",color: "white", borderRadius:"border-rounded",margin: "10px",padding: "1em 3em",
       backgroundSize: "200% auto",boxShadow:"0 4px 6px rgba(50,50,93,.11), 0 1px 3px rgba(0,0,0,.08)",
       backgroundImage: "linear-gradient(to right, #540772 0%, #9c0c89 50%, #22129b 100%)",
       transition: "0.5s"}}>Start</button>
       </Link>
  </div>
    
  );
};
  
export default First;
