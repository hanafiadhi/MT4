//+------------------------------------------------------------------+
//|                                            Custom Expert Advisor |
//|                    Copyright © 2025, Traders Family Indonesia    |
//|                         https://tradersfamily.id/                |
//+------------------------------------------------------------------+
#property copyright   "© 2025, Traders Family Indonesia"
#property link        "https://tradersfamily.id/"
#property description "Expert Advisor untuk pengelolaan posisi dan data trading secara efisien dan otomatis di MetaTrader 4"

#property strict
#property show_inputs

// Import DLL functions from the SQLite integrated DLL
#import "sqlite_32.dll"
   bool  openDatabase(const char &dbName[],        int &dbHandle);
   bool  closeDatabase(int dbHandle);
   bool  createTable(int dbHandle);
   bool  clearExposureLog(int dbHandle);
   bool  insertTradeBinary(int dbHandle,          const uchar &binaryData[], int dataSize);
   bool  upsertTradeBinary(int dbHandle,          const uchar &binaryData[], int dataSize);
   bool  insertExposureBinary(int dbHandle,       const uchar &binaryData[], int dataSize);

   void createTradeData(
      int         account,
      int         ticket,
      const char &symbol[],
      const char &type[],
      double      lots,
      double      open_price,
      double      stop_loss,
      double      take_profit,
      double      profit,
      const char &open_time[],
      const char &close_time[],
      uchar       &outTrade[]
   );

   void createExposureData(
      const char &snapshot_time[],
      const char &currency[],
      double      amount,
      double      rate_to_usd,
      double      usd_value,
      uchar       &outExposure[]
   );
#import

// Constants for struct sizes and limits
#define TRADE_DATA_SIZE      144  // sizeof(TradeData)
#define EXPOSURE_DATA_SIZE    56  // sizeof(ExposureData)
#define MAX_CURRENCIES        32
#define SYMBOL_SIZE           32
#define TYPE_SIZE             16
#define TIME_SIZE             24
#define CURRENCY_SIZE          8

// Global variables
int      dbHandle             = 0;
datetime lastTradeUpdate      = 0;
datetime lastExposureUpdate   = 0;

// Input parameters
input string DBFileName               = "traders_family.db";
input int    TradeUpdateInterval      = 5;   // #CCA (Interval HFT untuk Trade Terminal)
input int    ExposureUpdateInterval   = 30;  // #XSG (Interval Exposure)
input bool   LogClosedTrades          = false;

//+------------------------------------------------------------------+
//| Expert initialization                                           |
//+------------------------------------------------------------------+
int OnInit()
{
   string path = TerminalInfoString(TERMINAL_DATA_PATH) + "\\MQL4\\Files\\" + DBFileName;
   int fh = FileOpen("test_write.tmp", FILE_WRITE|FILE_BIN);
   if(fh == INVALID_HANDLE)
   {
      Print("❌ Cannot write to MQL4\\Files. Check permissions.");
      return INIT_FAILED;
   }
   FileClose(fh); FileDelete("test_write.tmp");

   char pathBuf[512]; ArrayInitialize(pathBuf,0);
   StringToCharArray(path, pathBuf, 0, 511);
   if(!openDatabase(pathBuf, dbHandle) || dbHandle<=0)
   {
      Print("❌ Failed to open database: ", path);
      return INIT_FAILED;
   }
   if(!createTable(dbHandle))
   {
      Print("❌ Failed to create tables");
      closeDatabase(dbHandle);
      return INIT_FAILED;
   }
   if(!clearExposureLog(dbHandle))
      Print("⚠️ Failed to clear exposure log");

   Print("✅ Database initialized: ", path);
   return INIT_SUCCEEDED;
}

//+------------------------------------------------------------------+
//| Expert deinitialization                                         |
//+------------------------------------------------------------------+
void OnDeinit(const int reason)
{
   if(dbHandle!=0)
   {
      if(closeDatabase(dbHandle))
         Print("✅ Database closed successfully");
      else
         Print("❌ Failed to close database");
   }
}

//+------------------------------------------------------------------+
//| Expert tick                                                     |
//+------------------------------------------------------------------+
void OnTick()
{
   datetime now = TimeLocal();
   if(now - lastTradeUpdate >= TradeUpdateInterval)
   {
      UpdateTrades();
      lastTradeUpdate = now;
   }
   if(now - lastExposureUpdate >= ExposureUpdateInterval)
   {
      UpdateExposure();
      lastExposureUpdate = now;
   }
}

//+------------------------------------------------------------------+
//| Update trades (open & optional closed)                          |
//+------------------------------------------------------------------+
void UpdateTrades()
{
   if(dbHandle<=0){ Print("❌ DB not connected."); return; }

   int openCount = OrdersTotal();
   if(openCount<=0) return;

   uchar buf[]; ArrayResize(buf, openCount*TRADE_DATA_SIZE); ArrayInitialize(buf,0);
   int count = 0;

   for(int i=0; i<openCount; i++)
   {
      if(!OrderSelect(i, SELECT_BY_POS, MODE_TRADES)) continue;
      int t = OrderType(); if(t>OP_SELL) continue;

      char sym[SYMBOL_SIZE];  ArrayInitialize(sym,0);
      char typ[TYPE_SIZE];    ArrayInitialize(typ,0);
      char ot[TIME_SIZE];     ArrayInitialize(ot,0);
      char ct[TIME_SIZE];     ArrayInitialize(ct,0);

      StringToCharArray(OrderSymbol(), sym, 0, SYMBOL_SIZE-1);
      StringToCharArray((t==OP_BUY)?"BUY":"SELL", typ, 0, TYPE_SIZE-1);
      StringToCharArray(TimeToString(OrderOpenTime(),TIME_DATE|TIME_SECONDS), ot, 0, TIME_SIZE-1);
      // ct empty for still-open orders

      uchar entry[]; ArrayResize(entry, TRADE_DATA_SIZE); ArrayInitialize(entry,0);
      createTradeData(
         AccountNumber(), OrderTicket(),
         sym, typ,
         OrderLots(), OrderOpenPrice(), OrderStopLoss(), OrderTakeProfit(), OrderProfit(),
         ot, ct,
         entry
      );
      ArrayCopy(buf, entry, count*TRADE_DATA_SIZE, 0, TRADE_DATA_SIZE);
      count++;
   }

   if(count>0)
   {
      ArrayResize(buf, count*TRADE_DATA_SIZE);
      PrintFormat("[DEBUG] DB=%d, BufferSize=%d, Count=%d", dbHandle, ArraySize(buf), count);
      if(upsertTradeBinary(dbHandle, buf, ArraySize(buf)))
         Print("✅ Updated ", count, " open trades.");
      else if(insertTradeBinary(dbHandle, buf, ArraySize(buf)))
         Print("✅ insertTradeBinary succeeded for ", count, " open trades.");
      else
         Print("❌ Failed writing open trades.");
   }

   // Closed trades logging (optional)
   if(LogClosedTrades)
   {
      int histCount = OrdersHistoryTotal();
      if(histCount>0)
      {
         uchar bufH[]; ArrayResize(bufH, histCount*TRADE_DATA_SIZE); ArrayInitialize(bufH,0);
         int countH=0;
         for(int i=0; i<histCount; i++)
         {
            if(!OrderSelect(i, SELECT_BY_POS, MODE_HISTORY)) continue;
            int t=OrderType();
            char sym[SYMBOL_SIZE]; ArrayInitialize(sym,0);
            char typ[TYPE_SIZE];   ArrayInitialize(typ,0);
            char ot[TIME_SIZE];    ArrayInitialize(ot,0);
            char ct[TIME_SIZE];    ArrayInitialize(ct,0);

            StringToCharArray(OrderSymbol(), sym, 0, SYMBOL_SIZE-1);
            StringToCharArray((t==OP_BUY)?"BUY":"SELL", typ, 0, TYPE_SIZE-1);
            StringToCharArray(TimeToString(OrderOpenTime(), TIME_DATE|TIME_SECONDS), ot, 0, TIME_SIZE-1);
            StringToCharArray(TimeToString(OrderCloseTime(),TIME_DATE|TIME_SECONDS), ct, 0, TIME_SIZE-1);

            uchar entry[]; ArrayResize(entry, TRADE_DATA_SIZE); ArrayInitialize(entry,0);
            createTradeData(
               AccountNumber(), OrderTicket(),
               sym, typ,
               OrderLots(), OrderOpenPrice(), OrderStopLoss(), OrderTakeProfit(), OrderProfit(),
               ot, ct,
               entry
            );
            ArrayCopy(bufH, entry, countH*TRADE_DATA_SIZE, 0, TRADE_DATA_SIZE);
            countH++;
         }
         if(countH>0)
         {
            ArrayResize(bufH, countH*TRADE_DATA_SIZE);
            if(upsertTradeBinary(dbHandle, bufH, ArraySize(bufH)))
               Print("✅ Logged ", countH, " closed trades.");
            else
               Print("❌ Failed logging closed trades.");
         }
      }
   }
}

//+------------------------------------------------------------------+
//| Update exposure snapshot                                         |
//+------------------------------------------------------------------+
void UpdateExposure()
{
   if(dbHandle<=0) return;

   string list[MAX_CURRENCIES];
   double vals[MAX_CURRENCIES]; ArrayInitialize(vals,0);
   int cnt = 0;

   // Hitung exposure per mata uang
   for(int i=0;i<OrdersTotal();i++)
   {
      if(!OrderSelect(i,SELECT_BY_POS,MODE_TRADES)) continue;
      int t=OrderType(); if(t>OP_SELL) continue;

      string s = OrderSymbol();
      string base, quote;
      int len = StringLen(s);

      // 1. Mapping khusus GOLD → XAU/USD
      if(s=="GOLD")
      {
         base  = "XAU";
         quote = "USD";
      }
      // 2. Simbol 6 huruf (3+3), misal EURUSD
      else if(len==6)
      {
         base  = StringSubstr(s,0,3);
         quote = StringSubstr(s,3,3);
      }
      // 3. Simbol 7 huruf (4+3), misal XAUUSD
      else if(len==7)
      {
         base  = StringSubstr(s,0,4);
         quote = StringSubstr(s,4,3);
      }
      // 4. Simbol 4 huruf lainnya (anggap base saja)
      else if(len==4)
      {
         base  = s;
         quote = "";
      }
      // 5. Fallback
      else
      {
         base  = StringSubstr(s,0,3);
         quote = (len>=6) ? StringSubstr(s,3,len-3) : "";
      }

      double lots  = OrderLots();
      double cs    = MarketInfo(s, MODE_LOTSIZE);
      double price = (t==OP_BUY) ? MarketInfo(s, MODE_BID) : MarketInfo(s, MODE_ASK);
      double vol   = lots * cs * ((t==OP_SELL)? -1 : 1);

      int idx = FindOrAddCurrency(base, list, vals, cnt);
      vals[idx] += vol;

      if(quote!="")
      {
         idx = FindOrAddCurrency(quote, list, vals, cnt);
         vals[idx] -= vol * price;
      }

      // Debug log per order
      PrintFormat("DEBUG EXP: %s → base=%s, quote=%s, lots=%.2f, CS=%.1f, vol=%.2f, price=%.5f",
                  s, base, quote, lots, cs, vol, price);
   }

   if(cnt==0) return;

   uchar buf[]; ArrayResize(buf, cnt*EXPOSURE_DATA_SIZE); ArrayInitialize(buf,0);
   char ts[TIME_SIZE]; ArrayInitialize(ts,0);
   StringToCharArray(TimeToString(TimeLocal(),TIME_DATE|TIME_SECONDS), ts, 0, TIME_SIZE-1);

   for(int i=0; i<cnt; i++)
   {
      char cu[CURRENCY_SIZE]; ArrayInitialize(cu,0);
      StringToCharArray(list[i], cu, 0, CURRENCY_SIZE-1);

      double rate = GetRateToUSD(list[i]);
      double uv   = vals[i] * rate;

      uchar entry[]; ArrayResize(entry, EXPOSURE_DATA_SIZE); ArrayInitialize(entry,0);
      createExposureData(ts, cu, vals[i], rate, uv, entry);
      ArrayCopy(buf, entry, i*EXPOSURE_DATA_SIZE, 0, EXPOSURE_DATA_SIZE);

      // Debug per currency
      PrintFormat("DEBUG EXPCUR: %s, amt=%.2f, rate=%.5f, usd_val=%.2f",
                  list[i], vals[i], rate, uv);
   }

   if(insertExposureBinary(dbHandle, buf, ArraySize(buf)))
      Print("✅ Logged exposure for ", cnt, " currencies.");
   else
      Print("❌ Failed logging exposure.");
}

//+------------------------------------------------------------------+
//| Helper: Find or add currency                                     |
//+------------------------------------------------------------------+
int FindOrAddCurrency(string c, string &l[], double &v[], int &n)
{
   for(int i=0;i<n;i++)
      if(l[i]==c) return i;

   if(n<MAX_CURRENCIES)
   {
      l[n] = c;
      return n++;
   }
   Print("⚠️ Currency array full.");
   return 0;
}

//+------------------------------------------------------------------+
//| Helper: Get exchange rate to USD                                 |
//+------------------------------------------------------------------+
double GetRateToUSD(string c)
{
   if(c=="USD") return 1.0;
   string p1 = c+"USD";
   double r1 = MarketInfo(p1, MODE_BID);
   if(r1>0) return r1;
   string p2 = "USD"+c;
   double r2 = MarketInfo(p2, MODE_BID);
   if(r2>0) return 1.0/r2;
   Print("⚠️ No rate for currency: ", c);
   return 1.0;
}
