+++
title = 'zeptoboard'
date = 2024-02-01T12:32:51-08:00
weight = 6
+++

# zeptoboard

zeptoboard is the breadboard variant of the zeptocore. It has most of the same functionality, but instead of using the buttons on the handheld device, you can utilize your keyboard and a midi interface (this website). This version requires some knowledge of breadboarding, but it is ideal if you want to develop your ideas based on the firmware.

you can purchase the components directly from me (helps to support my development!):


<div >
<div id='product-component-1715183469019' class="product"></div>
</div>
<script type="text/javascript">
(function () {
  var scriptURL = 'https://sdks.shopifycdn.com/buy-button/latest/buy-button-storefront.min.js';
  if (window.ShopifyBuy) {
    if (window.ShopifyBuy.UI) {
      ShopifyBuyInit();
    } else {
      loadScript();
    }
  } else {
    loadScript();
  }
  function loadScript() {
    var script = document.createElement('script');
    script.async = true;
    script.src = scriptURL;
    (document.getElementsByTagName('head')[0] || document.getElementsByTagName('body')[0]).appendChild(script);
    script.onload = ShopifyBuyInit;
  }
  function ShopifyBuyInit() {
    var client = ShopifyBuy.buildClient({
      domain: 'infinitedigits.myshopify.com',
      storefrontAccessToken: '9e045e3ce0fbee0fb64ebbbce133b648',
    });
    ShopifyBuy.UI.onReady(client).then(function (ui) {
      ui.createComponent('product', {
        id: '8985204588827',
        node: document.getElementById('product-component-1715183469019'),
        moneyFormat: '%24%7B%7Bamount%7D%7D',
        options: {
  "product": {
    "styles": {
      "product": {
        "@media (min-width: 601px)": {
          "max-width": "calc(25% - 20px)",
          "margin-left": "20px",
          "margin-bottom": "50px"
        }
      },
      "title": {
        "font-family": "Roboto, sans-serif"
      },
      "button": {
        "font-family": "Roboto, sans-serif"
      },
      "price": {
        "font-family": "Roboto, sans-serif"
      },
      "compareAt": {
        "font-family": "Roboto, sans-serif"
      },
      "unitPrice": {
        "font-family": "Roboto, sans-serif"
      }
    },
    "text": {
      "button": "Add to cart"
    },
    "googleFonts": [
      "Roboto"
    ]
  },
  "productSet": {
    "styles": {
      "products": {
        "@media (min-width: 601px)": {
          "margin-left": "-20px"
        }
      }
    }
  },
  "modalProduct": {
    "contents": {
      "img": false,
      "imgWithCarousel": true,
      "button": false,
      "buttonWithQuantity": true
    },
    "styles": {
      "product": {
        "@media (min-width: 601px)": {
          "max-width": "100%",
          "margin-left": "0px",
          "margin-bottom": "0px"
        }
      },
      "button": {
        "font-family": "Roboto, sans-serif"
      },
      "title": {
        "font-family": "Helvetica Neue, sans-serif",
        "font-weight": "bold",
        "font-size": "26px",
        "color": "#4c4c4c"
      },
      "price": {
        "font-family": "Helvetica Neue, sans-serif",
        "font-weight": "normal",
        "font-size": "18px",
        "color": "#4c4c4c"
      },
      "compareAt": {
        "font-family": "Helvetica Neue, sans-serif",
        "font-weight": "normal",
        "font-size": "15.299999999999999px",
        "color": "#4c4c4c"
      },
      "unitPrice": {
        "font-family": "Helvetica Neue, sans-serif",
        "font-weight": "normal",
        "font-size": "15.299999999999999px",
        "color": "#4c4c4c"
      }
    },
    "googleFonts": [
      "Roboto"
    ],
    "text": {
      "button": "Add to cart"
    }
  },
  "option": {
    "styles": {
      "label": {
        "font-family": "Roboto, sans-serif"
      },
      "select": {
        "font-family": "Roboto, sans-serif"
      }
    },
    "googleFonts": [
      "Roboto"
    ]
  },
  "cart": {
    "styles": {
      "button": {
        "font-family": "Roboto, sans-serif"
      }
    },
    "text": {
      "total": "Subtotal",
      "button": "Checkout"
    },
    "contents": {
      "note": true
    },
    "googleFonts": [
      "Roboto"
    ]
  },
  "toggle": {
    "styles": {
      "toggle": {
        "font-family": "Roboto, sans-serif"
      }
    },
    "googleFonts": [
      "Roboto"
    ]
  }
},
      });
    });
  }
})();
</script>

or directly from another manufacturer like Amazon:

- [WeAct Raspberry Pi Pico (others should work too)](https://www.aliexpress.us/item/3256803521775546.html?gatewayAdapt=glo2usa4itemAdapt) ($2.50)
- [pcm5102](https://www.amazon.com/Comimark-Interface-PCM5102-GY-PCM5102-Raspberry/dp/B07W97D2YC/) ($9)
- [sdio sd card](https://www.adafruit.com/product/4682) ($3.50)

 (_note:_ if you purchase components from me, I am able to provide reasonable amounts of support to help with your project).

<p style="text-align:center;">
<img src="/_core_bb.webp" style="width:90%; text-align:center; margin:auto 0; padding:1em; max-width:500px;">
</p>

## pcm5102

when you get these off-the-shelf pcm5102 chips, make sure that you solder the backside to ensure proper audio connections. see this diagram:

<div style="text-align:center;">
<img src="/img/pcm5102.webp" style="width:90%; text-align:center; margin:auto 0; padding:1em; max-width:500px;">
</div>
