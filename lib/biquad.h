#define LPF_MAX 45
int32_t x1_f, x2_f, y1_f, y2_f;
int32_t filter_lpf(int32_t x, int32_t f_, uint8_t q) {
  int32_t y;
int32_t f = f_;

  if (f>LPF_MAX) f=LPF_MAX;
  if (f==0) {
      y = 0.0021470287398510386 * x + 0.004294057479702077 * x1_f + 0.0021470287398510386 * x2_f - -1.9397197844518743 * y1_f - 0.9483078994112784 * y2_f; 
    }
  else if (f==1) {
      y = 0.0024060553103434704 * x + 0.004812110620686941 * x1_f + 0.0024060553103434704 * x2_f - -1.9357035281430215 * y1_f - 0.9453277493843956 * y2_f; 
    }
  else if (f==2) {
      y = 0.002696066164622393 * x + 0.005392132329244786 * x1_f + 0.002696066164622393 * x2_f - -1.9313978618133143 * y1_f - 0.9421821264718041 * y2_f; 
    }
  else if (f==3) {
      y = 0.003020716962851108 * x + 0.006041433925702216 * x1_f + 0.003020716962851108 * x2_f - -1.9267798064966881 * y1_f - 0.9388626743480926 * y2_f; 
    }
  else if (f==4) {
      y = 0.0033840850123729393 * x + 0.0067681700247458785 * x1_f + 0.0033840850123729393 * x2_f - -1.921824388776244 * y1_f - 0.935360728825736 * y2_f; 
    }
  else if (f==5) {
      y = 0.0037907160507436384 * x + 0.007581432101487277 * x1_f + 0.0037907160507436384 * x2_f - -1.9165044595757696 * y1_f - 0.9316673237787442 * y2_f; 
    }
  else if (f==6) {
      y = 0.004245675863575304 * x + 0.008491351727150608 * x1_f + 0.004245675863575304 * x2_f - -1.9107904970603333 * y1_f - 0.9277732005146345 * y2_f; 
    }
  else if (f==7) {
      y = 0.004754607167708111 * x + 0.009509214335416223 * x1_f + 0.004754607167708111 * x2_f - -1.9046503925150593 * y1_f - 0.9236688211858918 * y2_f; 
    }
  else if (f==8) {
      y = 0.005323792214782571 * x + 0.010647584429565142 * x1_f + 0.005323792214782571 * x2_f - -1.8980492180529258 * y1_f - 0.919344386912056 * y2_f; 
    }
  else if (f==9) {
      y = 0.005960221593245197 * x + 0.011920443186490394 * x1_f + 0.005960221593245197 * x2_f - -1.8909489749995765 * y1_f - 0.9147898613725574 * y2_f; 
    }
  else if (f==10) {
      y = 0.006671669727344726 * x + 0.013343339454689452 * x1_f + 0.006671669727344726 * x2_f - -1.883308321819762 * y1_f - 0.9099950007291409 * y2_f; 
    }
  else if (f==11) {
      y = 0.007466777588743029 * x + 0.014933555177486058 * x1_f + 0.007466777588743029 * x2_f - -1.8750822804908776 * y1_f - 0.9049493908458497 * y2_f; 
    }
  else if (f==12) {
      y = 0.008355143148771465 * x + 0.01671028629754293 * x1_f + 0.008355143148771465 * x2_f - -1.866221920299659 * y1_f - 0.899642492894745 * y2_f; 
    }
  else if (f==13) {
      y = 0.009347420105715765 * x + 0.01869484021143153 * x1_f + 0.009347420105715765 * x2_f - -1.8566740181445525 * y1_f - 0.8940636985674154 * y2_f; 
    }
  else if (f==14) {
      y = 0.01045542542019122 * x + 0.02091085084038244 * x1_f + 0.01045542542019122 * x2_f - -1.8463806945756231 * y1_f - 0.8882023962563882 * y2_f; 
    }
  else if (f==15) {
      y = 0.011692256180857655 * x + 0.02338451236171531 * x1_f + 0.011692256180857655 * x2_f - -1.8352790250037951 * y1_f - 0.8820480497272258 * y2_f; 
    }
  else if (f==16) {
      y = 0.013072416300375329 * x + 0.026144832600750657 * x1_f + 0.013072416300375329 * x2_f - -1.8233006257701212 * y1_f - 0.8755902909716224 * y2_f; 
    }
  else if (f==17) {
      y = 0.014611953505389897 * x + 0.029223907010779794 * x1_f + 0.014611953505389897 * x2_f - -1.810371215092799 * y1_f - 0.8688190291143586 * y2_f; 
    }
  else if (f==18) {
      y = 0.0163286070320574 * x + 0.0326572140641148 * x1_f + 0.0163286070320574 * x2_f - -1.7964101493142508 * y1_f - 0.8617245774424803 * y2_f; 
    }
  else if (f==19) {
      y = 0.01824196636766339 * x + 0.03648393273532678 * x1_f + 0.01824196636766339 * x2_f - -1.7813299353626748 * y1_f - 0.8542978008333284 * y2_f; 
    }
  else if (f==20) {
      y = 0.020373641286709536 * x + 0.04074728257341907 * x1_f + 0.020373641286709536 * x2_f - -1.7650357209319176 * y1_f - 0.8465302860787557 * y2_f; 
    }
  else if (f==21) {
      y = 0.02274744331394522 * x + 0.04549488662789044 * x1_f + 0.02274744331394522 * x2_f - -1.7474247645798457 * y1_f - 0.8384145378356267 * y2_f; 
    }
  else if (f==22) {
      y = 0.02538957860493768 * x + 0.05077915720987536 * x1_f + 0.02538957860493768 * x2_f - -1.728385888757457 * y1_f - 0.8299442031772076 * y2_f; 
    }
  else if (f==23) {
      y = 0.0283288520649934 * x + 0.0566577041299868 * x1_f + 0.0283288520649934 * x2_f - -1.7077989197163297 * y1_f - 0.8211143279763033 * y2_f; 
    }
  else if (f==24) {
      y = 0.031596882328243384 * x + 0.06319376465648677 * x1_f + 0.031596882328243384 * x2_f - -1.6855341193066038 * y1_f - 0.8119216486195773 * y2_f; 
    }
  else if (f==25) {
      y = 0.03522832698998429 * x + 0.07045665397996859 * x1_f + 0.03522832698998429 * x2_f - -1.6614516148751712 * y1_f - 0.8023649228351085 * y2_f; 
    }
  else if (f==26) {
      y = 0.03926111722747704 * x + 0.07852223445495408 * x1_f + 0.03926111722747704 * x2_f - -1.6354008348054743 * y1_f - 0.7924453037153825 * y2_f; 
    }
  else if (f==27) {
      y = 0.0437367006592025 * x + 0.087473401318405 * x1_f + 0.0437367006592025 * x2_f - -1.6072199587049323 * y1_f - 0.7821667613417423 * y2_f; 
    }
  else if (f==28) {
      y = 0.048700290983397516 * x + 0.09740058196679503 * x1_f + 0.048700290983397516 * x2_f - -1.5767353928404848 * y1_f - 0.771536556774075 * y2_f; 
    }
  else if (f==29) {
      y = 0.054201122608405615 * x + 0.10840224521681123 * x1_f + 0.054201122608405615 * x2_f - -1.5437612831433474 * y1_f - 0.7605657735769699 * y2_f; 
    }
  else if (f==30) {
      y = 0.06029270814621832 * x + 0.12058541629243665 * x1_f + 0.06029270814621832 * x2_f - -1.5080990799496117 * y1_f - 0.7492699125344849 * y2_f; 
    }
  else if (f==31) {
      y = 0.06703309629360699 * x + 0.13406619258721397 * x1_f + 0.06703309629360699 * x2_f - -1.469537170619831 * y1_f - 0.737669555794259 * y2_f; 
    }
  else if (f==32) {
      y = 0.074485127279208 * x + 0.148970254558416 * x1_f + 0.074485127279208 * x2_f - -1.4278505983096563 * y1_f - 0.7257911074264883 * y2_f; 
    }
  else if (f==33) {
      y = 0.08271668271423233 * x + 0.16543336542846465 * x1_f + 0.08271668271423233 * x2_f - -1.3828008874941617 * y1_f - 0.7136676183510909 * y2_f; 
    }
  else if (f==34) {
      y = 0.0918009263478813 * x + 0.1836018526957626 * x1_f + 0.0918009263478813 * x2_f - -1.3341359994763586 * y1_f - 0.7013397048678839 * y2_f; 
    }
  else if (f==35) {
      y = 0.10181653188384863 * x + 0.20363306376769727 * x1_f + 0.10181653188384863 * x2_f - -1.2815904442052004 * y1_f - 0.6888565717405949 * y2_f; 
    }
  else if (f==36) {
      y = 0.11284789363031518 * x + 0.22569578726063036 * x1_f + 0.11284789363031518 * x2_f - -1.2248855785731887 * y1_f - 0.6762771530944496 * y2_f; 
    }
  else if (f==37) {
      y = 0.124985315270425 * x + 0.24997063054085 * x1_f + 0.124985315270425 * x2_f - -1.1637301264122109 * y1_f - 0.6636713874939107 * y2_f; 
    }
  else if (f==38) {
      y = 0.1383251713424465 * x + 0.276650342684893 * x1_f + 0.1383251713424465 * x2_f - -1.0978209623635293 * y1_f - 0.6511216477333154 * y2_f; 
    }
  else if (f==39) {
      y = 0.15297003492293157 * x + 0.30594006984586314 * x1_f + 0.15297003492293157 * x2_f - -1.0268442117359189 * y1_f - 0.6387243514276453 * y2_f; 
    }
  else if (f==40) {
      y = 0.16902876321340593 * x + 0.33805752642681186 * x1_f + 0.16902876321340593 * x2_f - -0.9504767329879609 * y1_f - 0.6265917858415846 * y2_f; 
    }
  else if (f==41) {
      y = 0.1866165297734722 * x + 0.3732330595469444 * x1_f + 0.1866165297734722 * x2_f - -0.8683880709465799 * y1_f - 0.6148541900404686 * y2_f; 
    }
  else if (f==42) {
      y = 0.2058547873005038 * x + 0.4117095746010076 * x1_f + 0.2058547873005038 * x2_f - -0.7802430007806361 * y1_f - 0.6036621499826513 * y2_f; 
    }
  else if (f==43) {
      y = 0.2268711370324078 * x + 0.4537422740648156 * x1_f + 0.2268711370324078 * x2_f - -0.6857048301629421 * y1_f - 0.5931893782925733 * y2_f; 
    }
  else if (f==44) {
      y = 0.24979906838785 * x + 0.4995981367757 * x1_f + 0.24979906838785 * x2_f - -0.5844396973784693 * y1_f - 0.5836359709298693 * y2_f; 
    }
  else {
      y = 0.27477751285789426 * x + 0.5495550257157885 * x1_f + 0.27477751285789426 * x2_f - -0.47612220716699744 * y1_f - 0.5752322585985744 * y2_f; 
    }
 y=y>>20;

  x2_f = x1_f;
  x1_f = x;
  y2_f = y1_f;
  y1_f = y;
  return (int32_t)(y);

}
