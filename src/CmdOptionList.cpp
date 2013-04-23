/*
* CmdOptionList.cpp
*
*  Created on: 2011-12-29
*      Author: owentou(ŷ���)
*
* Ӧ�ó��������
*
*/

#include "CmdOptionList.h"

namespace copt
{
    CmdOptionList::CmdOptionList()
    {
    }

    CmdOptionList::CmdOptionList(int argv, const char* argc[])
    {
        for (int i = 0; i < argv; ++ i)
            m_stKeys.push_back(value_type(new CmdOptionValue(argc[i])));
    }

    CmdOptionList::CmdOptionList(const std::vector<std::string>& stCmds)
    {
        std::vector<std::string>::size_type uSize = stCmds.size();
        for (std::vector<std::string>::size_type i = 0; i < uSize; ++ i)
        {
            m_stKeys.push_back(value_type(new CmdOptionValue(stCmds[i].c_str())));
        }
    }


    void CmdOptionList::initKeyValueMap()
    {
        typedef std::map<std::string, value_type> key_map_type;
        typedef std::vector<value_type> keys_type;
        // �Ѿ���ʼ��������
        if (m_pKeyValue.get() != NULL)
            return;

        m_pKeyValue = std::shared_ptr<key_map_type>(new key_map_type());

        keys_type::size_type uLen = (m_stKeys.size() / 2) * 2; // ȥ��ĩβ��value��key
        for (keys_type::size_type i = 0; i < uLen; i += 2) 
        {
            (*m_pKeyValue)[m_stKeys[i]->AsString()] = m_stKeys[i + 1];
        }
    }

    void CmdOptionList::Add(const char* strParam)
    {
        m_stKeys.push_back(value_type(new CmdOptionValue(strParam)));
    }

    void CmdOptionList::Clear()
    {
        m_pKeyValue.reset();    // ɾ��key-valueӳ��
        m_stKeys.clear();       // ɾ�������±�ӳ��
    }

    CmdOptionList::value_type CmdOptionList::Get(std::string strKey, const char* strDefault)
    {
        value_type pRet = Get(strKey);
        if (pRet.get() == NULL)
            return value_type(new CmdOptionValue(strDefault));
        return pRet;
    }

    CmdOptionList::value_type CmdOptionList::Get(std::string strKey)
    {
        initKeyValueMap();

        std::map<std::string, value_type>::const_iterator itr;
        itr = m_pKeyValue->find(strKey);
        if (itr == m_pKeyValue->end())
            return value_type();
        return itr->second;
    }

    CmdOptionList::value_type CmdOptionList::Get(int iIndex) const
    {
        return m_stKeys[iIndex];
    }

    // ���������أ����ܺ�����һ��
    CmdOptionList::value_type CmdOptionList::operator[](int iIndex) const
    {
        return m_stKeys[iIndex];
    }

    // ��ȡ��������
    CmdOptionList::size_type CmdOptionList::GetParamsNumber() const
    {
        return m_stKeys.size();
    }

    // ����Key-Valueӳ���
    void CmdOptionList::ResetKeyValueMap()
    {
        m_pKeyValue.reset();
    }
}