
import React from "react";
import { Link } from 'react-router-dom';
import MainPage from "./MainPage";

  
const First = () => {
  return (
  <div>
  
    <div>
      <h1 style={{textAlign:"center"}}><b>Welcome to Apache Age Viewer</b></h1>
    </div>

    <div class="content" style={{
       display:"block", 
       width: "50%", 
       height: "50%", 
       margin:"auto",
       padding:"10% 0 0",
       backgroundPosition:"center",
       backgroundRepeat:"no-repeat",
       backgroundSize: "100% 100%",
       backgroundOrigin: "content-box",
       animation:"animate 3s linear infinite"}}>


      <img src="frontend\src\images\logo.png" id="logo" style={{  
       display: "block",
       width: "50%",
       height: "50%",
       margin: "auto",
       padding: "10% 0 0",
       backgroundPosition:"center",
       backgroundRepeat:"no-repeat",
       backgroundSize: "100% 100%",backgroundOrigin: "content-box"}} alt="logo"/>

    </div>
    <Link to="/second">
       <button class="popup-button" style={{textAlign:"center",textDecoration: "none",fontWeight: "800",fontSize: "1em",
       textTransform: "uppercase",color: "white", borderRadius:"border-rounded",margin: "10px",padding: "1em 3em",
       backgroundSize: "200% auto",boxShadow:"0 4px 6px rgba(50,50,93,.11), 0 1px 3px rgba(0,0,0,.08)",
       backgroundImage: "linear-gradient(to right, #540772 0%, #9c0c89 50%, #22129b 100%)",
       transition: "0.5s"}}>Start</button>
       </Link>
  </div>
    
  );
};
  
export default First;