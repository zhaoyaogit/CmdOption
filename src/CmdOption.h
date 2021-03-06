#ifndef _CMDOPTION_H_
#define _CMDOPTION_H_

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

/**
 * CmdOptionBind<TCmdStr>.h
 * 
 *  Version: 1.5.4
 *  Created on: 2011-12-29
 *  Last edit : 2014-04-18
 *      Author: OWenT
 *
 * 应用程序命令处理
 * 注意: 除了类绑定的目标类外，所有默认的函数推断均是传值方式引入
 *      并且数值的复制在执行BindCmd时，如果需要引用需要显式指定函数类型
 *      注意默认推导不支持隐式转换(即对double和int是默认推导，但float、short、long等需要指明参数类型)
 *      为了更高效，所有返回值均为PDO类型和指针/智能指针
 *
 */

#include <exception>
#include <set>
#include <cstdio>

#include <vector>
#include <map>


// 载入绑定器
#include "CmdOptionBind.h"
#include "CmdOptionString.h"
#include "CmdOptionBindTBase.h"

#include "ref.h"

namespace copt
{
    // 标准指令处理函数(无返回值，参数为选项的映射表)
    // void function_name (CmdOptionList&, [参数]); // 函数参数可选
    // void function_name (callback_param, [参数]); // 函数参数可选

    // 值类型
    typedef std::shared_ptr<copt::CmdOptionValue> value_type;


    /**
     * 命令处理函数
     * 内定命令/变量列表(用于处理内部事件):
     *      @OnError    :  出错时触发
     *          @ErrorMsg   : @OnError 函数的错误名称
     *
     *      @OnDefault  :  默认执行函数(用于执行批量命令时的第一个指令前的参数)
     *                     注意：如果第一个参数就是指令则@OnDefault会被传入空参数执行
     *
     *      @OnCallFunc :  分离参数后，转入命令前(将传入所有参数，仅限于子CmdOptionBind<TCmdStr>执行时)
     *                     建议：可以在这个事件响应函数里再绑定其他命令，减少指令冗余
     *                     [注: 调用Start函数不会响应这个事件]
     */
    template<typename TCmdStr>
    class CmdOptionBind: public binder::CmdOptionBindBase
    {
    protected:
        typedef unsigned char uc_t;
        static short m_strMapValue[256]; // 记录不同字符的映射关系
        static char m_strTransValue[256]; // 记录特殊转义字符

        typedef std::map<TCmdStr, std::shared_ptr<binder::CmdOptionBindBase> > funmap_type;
        funmap_type m_stCallbackFuns; // 记录命令的映射函数

        /**
         * 执行命令
         * @param strCmd 指令名称
         * @param stParams 指令参数
         */
        void runCmd(const TCmdStr& strCmd, callback_param stParams) const
        {
            typename funmap_type::const_iterator stIter = m_stCallbackFuns.find(strCmd);

            // 如果是顶层调用则添加根指令调用栈
            if (stParams.GetCmdArray().empty())
            {
                stParams.AppendCmd(ROOT_NODE_CMD, std::const_pointer_cast<binder::CmdOptionBindBase>(shared_from_this()));
            }

            if (stIter == m_stCallbackFuns.end())
            {
                // 内定命令不报“找不到指令”错
                if (strCmd == "@OnDefault")
                    return;

                stIter = m_stCallbackFuns.find("@OnError"); // 查找错误处理函数
                if (stIter != m_stCallbackFuns.end())
                {
                    // 末尾无参数Key填充空Value
                    if (stParams.GetParamsNumber() % 2)
                        stParams.Add("");

                    // 错误附加内容(错误内容)
                    stParams.Add("@ErrorMsg");
                    stParams.Add("Command Invalid");
                    stParams.AppendCmd(strCmd.c_str(), stIter->second); // 添加当前指令调用栈

                    (*stIter->second)(stParams);
                }
                return;
            }

            // 添加当前指令调用栈
            stParams.AppendCmd(strCmd.c_str(), stIter->second);
            (*stIter->second)(stParams);
        }

        /**
         * 从字符串获取一个字段（返回未处理的字符串结尾）
         * @param strBegin 要解析的字符串的起始位置
         * @param strVal 解析结果
         * @return 未解析部分的开始位置
         */
        const char* getSegment(const char* strBegin, std::string& strVal) const
        {
            strVal.clear();
            char cFlag;  // 字符串开闭字符

            // 去除分隔符前缀
            while (*strBegin && (m_strMapValue[(uc_t)*strBegin] & SPLITCHAR))
                ++ strBegin;

            while (*strBegin && !(m_strMapValue[(uc_t)*strBegin] & SPLITCHAR))
            {
                if (!(m_strMapValue[(uc_t)*strBegin] & STRINGSYM))
                {
                    strVal += *strBegin;
                    ++ strBegin;
                }
                else
                {
                    cFlag = *strBegin;
                    ++ strBegin;

                    while (*strBegin && *strBegin != cFlag)
                    {
                        char cCurByte = *strBegin;
                        if (m_strMapValue[(uc_t)*strBegin] & TRANSLATE)
                        {
                            if (*(strBegin + 1))
                            {
                                ++ strBegin;
                                cCurByte = m_strTransValue[(uc_t)*strBegin];
                            }
                        }

                        strVal += cCurByte;
                        ++ strBegin;
                    }

                    ++ strBegin;
                    break;  // 字符串结束后参数结束
                }
            }

            // 去除分隔符后缀
            while (*strBegin && (m_strMapValue[(uc_t)*strBegin] & SPLITCHAR))
                ++ strBegin;
            return strBegin;
        }

        /**
         * 多指令分离
         * @param strBegin 源字符串
         * @return 分离结果
         */
        std::vector<std::string> splitCmd(const char* strBegin) const
        {
            std::vector<std::string> stRet;
            for (const char* pBegin = strBegin; (*pBegin);)
            {
                std::string strCmd;
                // 去除命令分隔符前缀
                while ((*pBegin) && (m_strMapValue[(uc_t)*pBegin] & CMDSPLIT))
                    ++ pBegin;

                // 分离命令
                while ((*pBegin) && !(m_strMapValue[(uc_t)*pBegin] & CMDSPLIT))
                {
                    strCmd.push_back(*pBegin);
                    ++ pBegin;
                }

                if (strCmd.size() > 0)
                    stRet.push_back(strCmd);
            }

            return stRet;
        }

        /**
         * 默认帮助函数
         */
        void onHelp(callback_param)
        {
            puts("Help:");
            puts(GetHelpMsg().c_str());
        }

    private:
        /**
         * 构造函数
         */
        CmdOptionBind()
        {
            // 如果已初始化则跳过
            if (m_strMapValue[(uc_t)' '] & SPLITCHAR)
                return;

            // 分隔符
            m_strMapValue[(uc_t)' '] = m_strMapValue[(uc_t)'\t'] =
                m_strMapValue[(uc_t)'\r'] = m_strMapValue[(uc_t)'\n'] = SPLITCHAR;
            // 字符串开闭符
            m_strMapValue[(uc_t)'\''] = m_strMapValue[(uc_t)'\"'] = STRINGSYM;
            // 转义标记符
            m_strMapValue[(uc_t)'\\'] = TRANSLATE;
            // 指令分隔符
            m_strMapValue[(uc_t)' '] |= CMDSPLIT;
            m_strMapValue[(uc_t)','] = m_strMapValue[(uc_t)';'] = CMDSPLIT;

            // 转义字符设置
            for (int i = 0; i < 256; ++ i)
                m_strTransValue[i] = i;

            m_strTransValue[(uc_t)'0'] = '\0';
            m_strTransValue[(uc_t)'a'] = '\a';
            m_strTransValue[(uc_t)'b'] = '\b';
            m_strTransValue[(uc_t)'f'] = '\f';
            m_strTransValue[(uc_t)'r'] = '\r';
            m_strTransValue[(uc_t)'n'] = '\n';
            m_strTransValue[(uc_t)'t'] = '\t';
            m_strTransValue[(uc_t)'v'] = '\v';
            m_strTransValue[(uc_t)'\\'] = '\\';
            m_strTransValue[(uc_t)'\''] = '\'';
            m_strTransValue[(uc_t)'\"'] = '\"';
        }

    public:
        typedef std::shared_ptr<CmdOptionBind> ptr_type;
        static ptr_type Create()
        {
            return ptr_type(new CmdOptionBind());
        }

        /**
         * 获取已绑定的指令列表
         * @return 指令列表指针
         */
        std::shared_ptr<std::vector<const char*> > GetCmdNames() const
        {
            typename funmap_type::const_iterator iter = m_stCallbackFuns.begin();
            std::shared_ptr<std::vector<const char*> > pRet = 
                std::shared_ptr<std::vector<const char*> >(new std::vector<const char*>());
            while (iter != m_stCallbackFuns.end())
            {
                pRet->push_back(iter->first.c_str());
                ++ iter;
            }
            return pRet;
        }

        /**
         * 获取已绑定的指令对象
         * @param strCmdName 指令名称
         * @return 绑定的指令对象指针, 未找到返回空指针指针
         */
        std::shared_ptr<binder::CmdOptionBindBase> GetBindedCmd(const char* strCmdName) const
        {
            typename funmap_type::const_iterator iter = m_stCallbackFuns.find(strCmdName);
            if (iter == m_stCallbackFuns.end())
                return std::shared_ptr<binder::CmdOptionBindBase>();
            return iter->second;
        }

        /**
         * 处理指令
         * 说明： 在第一个指令前的参数都将传入@OnDefault事件
         *     参数可通过Get[数组下标]获取
         *     第一次使用Get[字符串]时将构建参数映射表，Get(0)为key，Get(1)为value，Get(2)为key，Get(3)为value，依此类推
         *     如果最后一组key没有value，执行Get[key]将返回空指针
         *     注意：Get[偶数下标]对应的所有value值和Get[字符串]返回的指针共享数据(即改了一个另一个也随之更改)
         */

        /**
         * 处理已分离指令(使用CmdOptionList存储参数集)
         * @param stArgs 数据集合
         * @param bSingleCmd 是否强制单指令, 如果不强制, 则指令名称不能重复
         */
        void Start(callback_param stArgs, bool bSingleCmd = false) const
        {
            int argv = static_cast<int>(stArgs.GetParamsNumber());
            CmdOptionList stCmdArgs;
            TCmdStr strCmd = bSingleCmd? "@OnError": "@OnDefault";
            for (int i = -1; i < argv; )
            {
                ++ i;
                stCmdArgs.Clear();
                stCmdArgs.LoadCmdArray(stArgs.GetCmdArray());
                stCmdArgs.SetExtParam(stArgs.GetExtParam());

                for (; i < argv; ++ i)
                {
                    // 把所有的非指令字符串设为指令参数
                    if (m_stCallbackFuns.find(stArgs[i]->AsString()) == m_stCallbackFuns.end())
                    {
                        stCmdArgs.Add(stArgs[i]->AsString());
                    }
                    else
                    {
                        // 如果是单指令且有未知参数则分发@OnError错误处理
                        if (bSingleCmd && stCmdArgs.GetParamsNumber() > 0)
                        {
                            runCmd(strCmd, stCmdArgs);
                            stCmdArgs.Clear();
                            stCmdArgs.LoadCmdArray(stArgs.GetCmdArray());
                            stCmdArgs.SetExtParam(stArgs.GetExtParam());
                        }

                        // 追加所有参数，执行单指令
                        if (bSingleCmd)
                        {
                            strCmd = TCmdStr(stArgs[i]->AsCppString().c_str(), stArgs[i]->AsCppString().size());
                            for(++ i; i < argv; ++ i)
                                stCmdArgs.Add(stArgs[i]->AsString());
                        }
                        break;
                    }
                }

                runCmd(strCmd, stCmdArgs);
                if (i >= argv)
                    break;
                strCmd = TCmdStr(stArgs[i]->AsCppString().c_str(), stArgs[i]->AsCppString().size());
            }
        }

        /**
         * 处理已分离指令(使用char**存储参数集)
         * @param argv 参数个数
         * @param argc 参数列表
         * @param bSingleCmd 是否强制单指令, 如果不强制, 则指令名称不能重复
         * @param pExtParam 透传参数
         */
        inline void Start(int argv, const char* argc[], bool bSingleCmd = false, void* pExtParam = NULL) const
        {
            CmdOptionList stList(argv, argc);
            stList.SetExtParam(pExtParam);

            Start(stList, bSingleCmd);
        }

        /**
         * 处理已分离指令(使用std::vector<std::string>存储参数集)
         * @param stCmds 数据集合
         * @param bSingleCmd 是否强制单指令, 如果不强制, 则指令名称不能重复
         * @param pExtParam 透传参数
         */
        inline void Start(const std::vector<std::string>& stCmds, bool bSingleCmd = false, void* pExtParam = NULL) const
        {
            CmdOptionList stList(stCmds);
            stList.SetExtParam(pExtParam);

            Start(stList, bSingleCmd);
        }
        
        /**
         * 处理未分离指令(使用const char*存储参数集字符串)
         * @param strCmd 指令
         * @param bSingleCmd 是否强制单指令, 如果不强制, 则指令名称不能重复
         */
        void Start(const char* strCmd, bool bSingleCmd = false, void* pExtParam = NULL) const
        {
            CmdOptionList stCmds;
            std::string strSeg;

            // 分离指令
            while (*strCmd)
            {
                strCmd = getSegment(strCmd, strSeg);
                stCmds.Add(strSeg.c_str());
            }
        
            stCmds.SetExtParam(pExtParam);

            Start(stCmds, bSingleCmd);
        }

        /**
         * 处理未分离指令(使用const std::string&存储参数集字符串)
         * @param strCmd 指令
         * @param bSingleCmd 是否强制单指令, 如果不强制, 则指令名称不能重复
         */
        inline void Start(const std::string& strCmd, bool bSingleCmd = false, void* pExtParam = NULL) const
        {
            Start(strCmd.c_str(), bSingleCmd, pExtParam);
        }

        /**
         * 解绑指令
         * @param strCmd 指令名称
         */
        inline void UnBindCmd(const std::string& strCmd)
        {
            TCmdStr strTCmd = TCmdStr(strCmd.c_str(), strCmd.size());
            m_stCallbackFuns.erase(strTCmd);
        }

        /**
         * 解绑全部指令
         */
        inline void UnBindAllCmd()
        {
            m_stCallbackFuns.clear();
        }

        /**
         * 绑定默认帮助函数
         * @param strHelpCmd 帮助命令名称
         */
        inline std::shared_ptr< binder::CmdOptionBindT<
            typename binder::MaybeWrapMemberPointer<
                void (CmdOptionBind<TCmdStr>::*) (callback_param)>::caller_type,
#if defined(COPT_ENABLE_VARIADIC_TEMPLATE)
                binder::CmdOptionBindParamList< CmdOptionBind<TCmdStr>* >
#else
                binder::CmdOptionBindParamList1< CmdOptionBind<TCmdStr>* >
#endif
        > > 
            BindHelpCmd(const char* strHelpCmd)
        {
            return BindCmd(strHelpCmd, &CmdOptionBind<TCmdStr>::onHelp, this);
        }

        /**
         * 执行子结构
         */
        virtual void operator()(callback_param arg)
        {
            // 响应@OnCallFunc事件
            typename funmap_type::const_iterator iter = m_stCallbackFuns.find("@OnCallFunc");
            if (iter != m_stCallbackFuns.end())
                (*iter->second)(arg);
        
            // 重新执行指令集, 进入子结构的一定是单指令
            Start(arg, true);
        }

        /**
         * 获取命令集合的帮助信息
         * @param strPre 前缀
         */
        virtual std::string GetHelpMsg(const char* strPre = "")
        {
            std::set<typename funmap_type::mapped_type> stSet;
            std::string strHelpMsg;

            for (typename funmap_type::const_iterator iter = m_stCallbackFuns.begin(); iter != m_stCallbackFuns.end(); ++ iter)
            {
                // 删除重复的引用对象
                if (stSet.find(iter->second) != stSet.end())
                    continue;

                // 跳过内置命令
                if ('@' == *iter->first.c_str())
                    continue;

                stSet.insert(iter->second);
                std::string strCmdHelp = iter->second->GetHelpMsg((strPre + m_strHelpMsg).c_str());

                if (strCmdHelp.size() > 0)
                {
                    if (strHelpMsg.size() > 0 && '\n' != *strHelpMsg.rbegin())
                        strHelpMsg += "\r\n"; 
                    strHelpMsg += strCmdHelp;
                }
            }
            return strHelpMsg;
        }

        /**
         * 增加指令处理函数 (相同命令会被覆盖)
         * 支持普通函数和类成员函数
         * 注意：所有传入的类为引用，请确保在执行Start时类对象未被释放（特别注意指针和局部变量）
         * 注意：参数的复制发生在执行BindCmd函数时
         */

        /**
         * 绑定函数对象/函数/成员函数(自适应)
         * 注意：默认会复制函数对象和传入参数
         *
         * BindCmd: 绑定参数[注意值的复制发生在本函数执行时]
         * example:
         *      *.BindCmd(命令名称, 函数对象/函数/成员函数, 参数)                           // 默认类型推断是传值而非引用
         *      *.BindCmd<传入类型>(命令名称, 函数对象/函数/成员函数, 参数)
         *      *.BindCmd<传入类型, 参数类型>(命令名称, 函数对象/函数/成员函数, 参数)
         */

#if defined(COPT_ENABLE_VARIADIC_TEMPLATE)
        template<typename _F, typename... _Args>  // 绑定函数(_Arg:参数[注意值的复制发生在本函数执行时], _R: 绑定函数返回值类型)
        std::shared_ptr<binder::CmdOptionBindT<
            typename binder::MaybeWrapMemberPointer<_F>::caller_type,
            binder::CmdOptionBindParamList<_Args...>
        > >
        BindCmd(const std::string& strCmd, _F fn, _Args... args)
        {
            typedef binder::CmdOptionBindParamList<_Args...> list_type;
            typedef typename binder::MaybeWrapMemberPointer<_F>::caller_type caller_type;
            typedef std::shared_ptr<binder::CmdOptionBindT<caller_type, list_type> > obj_type;

            obj_type pFun = obj_type(new binder::CmdOptionBindT<caller_type, list_type>(caller_type(fn), list_type(args...)));

            std::vector<std::string> stCmds = splitCmd(strCmd.c_str());
            for (std::vector<std::string>::size_type iIndex = 0; iIndex < stCmds.size(); ++ iIndex)
            {
                TCmdStr strTCmd = TCmdStr(stCmds[iIndex].c_str(), stCmds[iIndex].size());
                m_stCallbackFuns[strTCmd] = pFun;
            }

            return pFun;
        }

#else
        template<typename _F>   // 绑定函数(_F: 函数对象类型)
        std::shared_ptr<binder::CmdOptionBindT<
            typename binder::MaybeWrapMemberPointer<_F>::caller_type,
            binder::CmdOptionBindParamList0
        > > 
            BindCmd(const std::string& strCmd, _F fn)
        {
            typedef binder::CmdOptionBindParamList0 list_type;
            typedef typename binder::MaybeWrapMemberPointer<_F>::caller_type caller_type;
            typedef std::shared_ptr<binder::CmdOptionBindT<caller_type, list_type> > obj_type;

            obj_type pFun = obj_type(new binder::CmdOptionBindT<caller_type, list_type>(caller_type(fn), list_type()));

            std::vector<std::string> stCmds = splitCmd(strCmd.c_str());
            for (std::vector<std::string>::size_type iIndex = 0; iIndex < stCmds.size(); ++ iIndex)
            {
                TCmdStr strTCmd = TCmdStr(stCmds[iIndex].c_str(), stCmds[iIndex].size());
                m_stCallbackFuns[strTCmd] = pFun;
            }

            return pFun;
        }

        template<typename _F, typename _Arg0>  // 绑定函数(_Arg:参数[注意值的复制发生在本函数执行时], _R: 绑定函数返回值类型)
        std::shared_ptr<binder::CmdOptionBindT<
            typename binder::MaybeWrapMemberPointer<_F>::caller_type,
            binder::CmdOptionBindParamList1<_Arg0> 
        > >
            BindCmd(const std::string& strCmd, _F fn, _Arg0 arg0)
        {
            typedef binder::CmdOptionBindParamList1<_Arg0> list_type;
            typedef typename binder::MaybeWrapMemberPointer<_F>::caller_type caller_type;
            typedef std::shared_ptr<binder::CmdOptionBindT<caller_type, list_type> > obj_type;

            obj_type pFun = obj_type(new binder::CmdOptionBindT<caller_type, list_type>(caller_type(fn), list_type(arg0)));

            std::vector<std::string> stCmds = splitCmd(strCmd.c_str());
            for (std::vector<std::string>::size_type iIndex = 0; iIndex < stCmds.size(); ++ iIndex)
            {
                TCmdStr strTCmd = TCmdStr(stCmds[iIndex].c_str(), stCmds[iIndex].size());
                m_stCallbackFuns[strTCmd] = pFun;
            }

            return pFun;
        }

        template<typename _F, typename _Arg0, typename _Arg1>  // 绑定函数(_Arg:参数[注意值的复制发生在本函数执行时], _R: 绑定函数返回值类型)
        std::shared_ptr<binder::CmdOptionBindT<
            typename binder::MaybeWrapMemberPointer<_F>::caller_type,
            binder::CmdOptionBindParamList2<_Arg0, _Arg1> 
        > >
            BindCmd(const std::string& strCmd, _F fn, _Arg0 arg0, _Arg1 arg1)
        {
            typedef binder::CmdOptionBindParamList2<_Arg0, _Arg1> list_type;
            typedef typename binder::MaybeWrapMemberPointer<_F>::caller_type caller_type;
            typedef std::shared_ptr<binder::CmdOptionBindT<caller_type, list_type> > obj_type;

            obj_type pFun = obj_type(new binder::CmdOptionBindT<caller_type, list_type>(caller_type(fn), list_type(arg0, arg1)));

            std::vector<std::string> stCmds = splitCmd(strCmd.c_str());
            for (std::vector<std::string>::size_type iIndex = 0; iIndex < stCmds.size(); ++ iIndex)
            {
                TCmdStr strTCmd = TCmdStr(stCmds[iIndex].c_str(), stCmds[iIndex].size());
                m_stCallbackFuns[strTCmd] = pFun;
            }

            return pFun;
        }

        template<typename _F, typename _Arg0, typename _Arg1, typename _Arg2>  // 绑定函数(_Arg:参数[注意值的复制发生在本函数执行时], _R: 绑定函数返回值类型)
        std::shared_ptr<binder::CmdOptionBindT<
            typename binder::MaybeWrapMemberPointer<_F>::caller_type,
            binder::CmdOptionBindParamList3<_Arg0, _Arg1, _Arg2> 
        > >
            BindCmd(const std::string& strCmd, _F fn, _Arg0 arg0, _Arg1 arg1, _Arg2 arg2)
        {
            typedef binder::CmdOptionBindParamList3<_Arg0, _Arg1, _Arg2> list_type;
            typedef typename binder::MaybeWrapMemberPointer<_F>::caller_type caller_type;
            typedef std::shared_ptr<binder::CmdOptionBindT<caller_type, list_type> > obj_type;

            obj_type pFun = obj_type(new binder::CmdOptionBindT<caller_type, list_type>(caller_type(fn), list_type(arg0, arg1, arg2)));

            std::vector<std::string> stCmds = splitCmd(strCmd.c_str());
            for (std::vector<std::string>::size_type iIndex = 0; iIndex < stCmds.size(); ++ iIndex)
            {
                TCmdStr strTCmd = TCmdStr(stCmds[iIndex].c_str(), stCmds[iIndex].size());
                m_stCallbackFuns[strTCmd] = pFun;
            }

            return pFun;
        }

        template<typename _F, typename _Arg0, typename _Arg1, typename _Arg2, typename _Arg3>  // 绑定函数(_Arg:参数[注意值的复制发生在本函数执行时], _R: 绑定函数返回值类型)
        std::shared_ptr<binder::CmdOptionBindT<
            typename binder::MaybeWrapMemberPointer<_F>::caller_type,
            binder::CmdOptionBindParamList4<_Arg0, _Arg1, _Arg2, _Arg3>
        > >
        BindCmd(const std::string& strCmd, _F fn, _Arg0 arg0, _Arg1 arg1, _Arg2 arg2, _Arg3 arg3)
        {
            typedef binder::CmdOptionBindParamList4<_Arg0, _Arg1, _Arg2, _Arg3> list_type;
            typedef typename binder::MaybeWrapMemberPointer<_F>::caller_type caller_type;
            typedef std::shared_ptr<binder::CmdOptionBindT<caller_type, list_type> > obj_type;

            obj_type pFun = obj_type(new binder::CmdOptionBindT<caller_type, list_type>(caller_type(fn), list_type(arg0, arg1, arg2, arg3)));

            std::vector<std::string> stCmds = splitCmd(strCmd.c_str());
            for (std::vector<std::string>::size_type iIndex = 0; iIndex < stCmds.size(); ++iIndex)
            {
                TCmdStr strTCmd = TCmdStr(stCmds[iIndex].c_str(), stCmds[iIndex].size());
                m_stCallbackFuns[strTCmd] = pFun;
            }

            return pFun;
        }
#endif

        /**
         * 绑定指令(通用)
         * BindCmd: 绑定参数
         * example:
         *      *.BindCmd(命令名称, binder::CmdOptionBindBase 结构智能指针)
         *      *.BindCmd(命令名称, CmdOptionBind<TCmdStr> 结构引用)
         * 推荐使用上一种，可以减少一次结构复制
         */
        std::shared_ptr<binder::CmdOptionBindBase> BindChildCmd(const std::string strCmd, std::shared_ptr<binder::CmdOptionBindBase> pBase)
        {
            std::vector<std::string> stCmds = splitCmd(strCmd.c_str());
            for (std::vector<std::string>::size_type iIndex = 0; iIndex < stCmds.size(); ++ iIndex)
            {
                TCmdStr strTCmd = TCmdStr(stCmds[iIndex].c_str(), stCmds[iIndex].size());
                m_stCallbackFuns[strTCmd] = pBase;
            }

            return pBase;
        }
        std::shared_ptr<binder::CmdOptionBindBase> BindChildCmd(const std::string strCmd, const CmdOptionBind<TCmdStr>& stCmdOpt)
        {
            std::shared_ptr<binder::CmdOptionBindBase> pBase = std::shared_ptr<CmdOptionBind<TCmdStr> >(new CmdOptionBind<TCmdStr>(stCmdOpt));
            std::vector<std::string> stCmds = splitCmd(strCmd.c_str());
            for (std::vector<std::string>::size_type iIndex = 0; iIndex < stCmds.size(); ++ iIndex)
            {
                TCmdStr strTCmd = TCmdStr(stCmds[iIndex].c_str(), stCmds[iIndex].size());
                m_stCallbackFuns[strTCmd] = pBase;
            }

            return pBase;
        }
    };

    template<typename Ty>
    short CmdOptionBind<Ty>::m_strMapValue[256] = {0};

    template<typename Ty>
    char CmdOptionBind<Ty>::m_strTransValue[256] = {0};

    // 类型重定义
    typedef CmdOptionBind<std::string> CmdOption;
    typedef CmdOptionBind<CmdOptionCIString> CmdOptionCI;
}

#endif /* CMDOPTION_H_ */
