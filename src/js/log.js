const EventEmitter = require('events')
const { styleText } = require('node:util')

/**
*   Log object class. EventEmitter.
*   uses 'chalk' for colorizing output
*   has methods log, test, error and success
*/
class Log extends EventEmitter
{

  constructor()
  {
    super()
    this._format = 'dd/mm/yyyy hh:ii:ss'
    this._pretty=false
    //this._pretty=true
    this._on = {
      log:true,
      warn: true,
      test: true,
      success: true,
      error: true,
      info: true
    }
  }

  setOuts(outs)
  {
    if (outs)
    this._on = outs
  }

  m(m)
  {
    let ret = {
      l: this,
      log: (...a)=>{ this.log(m,...a) },
      info: (...a)=>{ this.info(m,...a) },
      test: (...a)=>{ this.test(m,...a) },
      warn: (...a)=>{ this.warn(m,...a) },
      error: (...a)=>{ this.error(m,...a) },
      success: (...a)=>{ this.success(m,...a) }
    }
    return ret
  }


  date()
  {
    let dt = new Date()
    let dd = dt.getDay().toString()
    let mm = (dt.getMonth()+1).toString()
    let yy = dt.getFullYear().toString()
    let hh = dt.getHours().toString()
    let ii = dt.getMinutes().toString()
    let ss = dt.getSeconds().toString()
    dd = dd.length > 1 ? dd : '0'+dd
    mm = mm.length > 1 ? mm : '0'+mm
    hh = hh.length > 1 ? hh : '0'+hh
    ii = ii.length > 1 ? ii : '0'+ii
    ss = ss.length > 1 ? ss : '0'+ss
    let str = this._format.trim()
    str=str.replace(/dd/g,dd)
    str=str.replace(/mm/g,mm)
    str=str.replace(/yyyy/g,yy)
    str=str.replace(/hh/g,hh)
    str=str.replace(/ii/g,ii)
    str=str.replace(/ss/g,ss)
    return str
  }

  vars(defKey,key,msg=null,obj=null)
  {
    let ret = {
      key: defKey,
      msg: '',
      obj: null
    }
    if (!msg) {
      ret.msg = key 
    } else {
      if (typeof msg != 'string')
      {
        ret.msg = key
        ret.obj = msg
      } else {
        if (msg)
        {
          ret.key = key
          ret.msg = msg
        }  else {
          ret.msg = key
        }
        if (obj)
          ret.obj = obj
      }
    }
    return ret
  }

  varst(defKey,tdata,key=null,msg=null,obj=null)
  {

  }

  _clog(tp,msg,obj=null)
  {
    if(!this._on[tp])
      return
    if (obj)
    {
      if (this._pretty) {
        msg += ' <'+JSON.stringify(obj,null,1)+'>'
        console.log(msg)
      } else {
        console.log(msg, obj)
      }
    } else {
      console.log(msg)
    }
  }
  /**
  *   colorize output
  *   @param {string} color - color to colorize
  *   @param {object} vars - message data
  **/
  _colorize(color, vars) {
    return styleText('white', this.date())+' ['+styleText(color, vars.key)+'] '+styleText('dim', vars.msg)
  }
  /**
  *   log method
  *   @param {string} key - key or message string. if there is no msg, or msg is not a string - param used as message
  *   @param {string/object} msg - message string. or debug object
  *   @param {object} obj - debug object
  */
  log(key,msg=null,obj=null)
  {
    let vars = this.vars('..',key,msg,obj)
    this.emit('log',vars)
    // let str = chalk.white(this.date())+' ['+chalk.blue(vars.key)+'] '+chalk.dim(vars.msg)
    this._clog('log',this._colorize('blue', vars),vars.obj)
  }

  /**
  *   info method
  *   @param {string} key - key or message string. if there is no msg, or msg is not a string - param used as message
  *   @param {string/object} msg - message string. or debug object
  *   @param {object} obj - debug object
  */
  info(key,msg=null,obj=null)
  {
    let vars = this.vars('..',key,msg,obj)
    this.emit('info',vars)
    // let str = chalk.white(this.date())+' ['+chalk.cyan(vars.key)+'] '+chalk.dim(vars.msg)
    this._clog('info',this._colorize('cyan', vars),vars.obj)
  }


  /**
  *   test method
  *   @param {string} key - key or message string. if there is no msg, or msg is not a string - param used as message
  *   @param {string/object} msg - message string. or debug object
  *   @param {object} obj - debug object
  */
  test(key,msg=null,obj=null)
  {
    let vars = this.vars('??',key,msg,obj)
    this.emit('test',vars)
    // let str = chalk.white(this.date())+' ['+chalk.magenta(vars.key)+'] '+chalk.dim(vars.msg)
    this._clog('test',this._colorize('magenta', vars),vars.obj)
  }

  /**
  *   warn method
  *   @param {string} key - key or message string. if there is no msg, or msg is not a string - param used as message
  *   @param {string/object} msg - message string. or debug object
  *   @param {object} obj - debug object
  */
  warn(key,msg=null,obj=null)
  {
    let vars = this.vars('!!',key,msg,obj)
    this.emit('warn',vars)
    // let str = chalk.white(this.date())+' ['+chalk.yellow(vars.key)+'] '+chalk.dim(vars.msg)
    this._clog('warn',this._colorize('yellow', vars),vars.obj)
  }

  /**
  *   error method
  *   @param {string} key - key or message string. if there is no msg, or msg is not a string - param used as message
  *   @param {string/object} msg - message string. or debug object
  *   @param {object} obj - debug object
  */
  error(key,msg=null,obj=null)
  {
    let vars = this.vars('!!',key,msg,obj)
    this.emit('error',vars)
    // let str = chalk.white(this.date())+' ['+chalk.red(vars.key)+'] '+chalk.dim(vars.msg)
    this._clog('error',this._colorize('red', vars),vars.obj)
  }

  /**
  *   success method
  *   @param {string} key - key or message string. if there is no msg, or msg is not a string - param used as message
  *   @param {string/object} msg - message string. or debug object
  *   @param {object} obj - debug object
  */
  success(key,msg=null,obj=null)
  {
    let vars = this.vars('ok',key,msg,obj)
    this.emit('success',vars)
    // let str = chalk.white(this.date())+' ['+chalk.green(vars.key)+'] '+chalk.dim(vars.msg)
    this._clog('success',this._colorize('green', vars),vars.obj)
  }

}

let log = new Log()

module.exports = log
