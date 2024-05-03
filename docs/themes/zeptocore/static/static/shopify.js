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
                id: '8831942492443',
                node: document.getElementById('product-component-1714750914504'),
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