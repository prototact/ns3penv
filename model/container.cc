/*
 * Copyright (c) 2018 Piotr Gawlowicz
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Piotr Gawlowicz <gawlowicz.p@gmail.com>
 * Modify: Muyuan Shen <muyuan_shen@hust.edu.cn>
 * Modify: Orfeas Karachalios <okarachalios@iit.demokritos.gr>
 *
 */

#include "container.h"

#include <ns3/log.h>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("OpenGymDataContainer");

NS_OBJECT_ENSURE_REGISTERED(OpenGymDataContainer);

TypeId
OpenGymDataContainer::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::OpenGymDataContainer").SetParent<Object>().SetGroupName("OpenGym");
    return tid;
}

OpenGymDataContainer::OpenGymDataContainer()
{
    // NS_LOG_FUNCTION (this);
}

OpenGymDataContainer::~OpenGymDataContainer()
{
    // NS_LOG_FUNCTION (this);
}

void
OpenGymDataContainer::DoDispose()
{
    // NS_LOG_FUNCTION (this);
}

void
OpenGymDataContainer::DoInitialize()
{
    // NS_LOG_FUNCTION (this);
}

Ptr<OpenGymDataContainer>
OpenGymDataContainer::CreateFromDataContainerPbMsg(ns3penv::DataContainer& dataContainerPbMsg)
{
    Ptr<OpenGymDataContainer> actDataContainer;

    switch (dataContainerPbMsg.data_case())
    {
    case ns3penv::DataContainer::kDiscrete: {
        const auto& discreteMsg = dataContainerPbMsg.discrete();

        Ptr<OpenGymDiscreteContainer> discrete = CreateObject<OpenGymDiscreteContainer>();
        discrete->SetValue(discreteMsg.data());
        actDataContainer = discrete;
        break;
    }
    case ns3penv::DataContainer::kBox: {
        const auto& boxMsg = dataContainerPbMsg.box();

        switch (boxMsg.dtype())
        {
        case ns3penv::INT: {
            Ptr<OpenGymBoxContainer<int32_t>> box = CreateObject<OpenGymBoxContainer<int32_t>>();
            std::vector<int32_t> myData{boxMsg.intdata().begin(), boxMsg.intdata().end()};
            box->SetData(myData);
            actDataContainer = box;
            break;
        }
        case ns3penv::UINT: {
            Ptr<OpenGymBoxContainer<uint32_t>> box = CreateObject<OpenGymBoxContainer<uint32_t>>();
            std::vector<uint32_t> myData{boxMsg.uintdata().begin(), boxMsg.uintdata().end()};
            box->SetData(myData);
            actDataContainer = box;
            break;
        }
        case ns3penv::FLOAT: {
            Ptr<OpenGymBoxContainer<float>> box = CreateObject<OpenGymBoxContainer<float>>();
            std::vector<float> myData{boxMsg.floatdata().begin(), boxMsg.floatdata().end()};
            box->SetData(myData);
            actDataContainer = box;
            break;
        }
        case ns3penv::DOUBLE: {
            Ptr<OpenGymBoxContainer<double>> box = CreateObject<OpenGymBoxContainer<double>>();
            std::vector<double> myData{boxMsg.doubledata().begin(), boxMsg.doubledata().end()};
            box->SetData(myData);
            actDataContainer = box;
            break;
        }
        default: {
            NS_ABORT_MSG("Unsupported data type");
            break;
        }
        }
        break;
    }
    case ns3penv::DataContainer::kTuple: {
        Ptr<OpenGymTupleContainer> tupleData = CreateObject<OpenGymTupleContainer>();

        const auto& tupleMsg = dataContainerPbMsg.tuple();
        std::vector<ns3penv::DataContainer> elements;
        elements.assign(tupleMsg.element().begin(), tupleMsg.element().end());

        for (auto& element : elements)
        {
            Ptr<OpenGymDataContainer> subData =
                OpenGymDataContainer::CreateFromDataContainerPbMsg(element);
            tupleData->Add(subData);
        }

        actDataContainer = tupleData;
        break;
    }
    case ns3penv::DataContainer::kDict: {
        Ptr<OpenGymDictContainer> dictData = CreateObject<OpenGymDictContainer>();
        const auto& dictMsg = dataContainerPbMsg.dict();

        std::vector<ns3penv::DataContainer> elements{dictMsg.element().begin(),
                                                     dictMsg.element().end()};

        for (auto& element : elements)
        {
            Ptr<OpenGymDataContainer> subSpace =
                OpenGymDataContainer::CreateFromDataContainerPbMsg(element);
            dictData->Add(element.name(), subSpace);
        }

        actDataContainer = dictData;
        break;
    }
    default:
        NS_ABORT_MSG("Unsupported data container type");
        break;
    }
    return actDataContainer;
}

TypeId
OpenGymDiscreteContainer::GetTypeId()
{
    static TypeId tid = TypeId("ns3::OpenGymDiscreteContainer")
                            .SetParent<OpenGymDataContainer>()
                            .SetGroupName("OpenGym")
                            .AddConstructor<OpenGymDiscreteContainer>();
    return tid;
}

OpenGymDiscreteContainer::OpenGymDiscreteContainer()
{
    // NS_LOG_FUNCTION (this);
    m_n = 0;
}

OpenGymDiscreteContainer::OpenGymDiscreteContainer(uint32_t n)
{
    // NS_LOG_FUNCTION (this);
    m_n = n;
}

OpenGymDiscreteContainer::~OpenGymDiscreteContainer()
{
    // NS_LOG_FUNCTION (this);
}

void
OpenGymDiscreteContainer::DoDispose()
{
    // NS_LOG_FUNCTION (this);
}

void
OpenGymDiscreteContainer::DoInitialize()
{
    // NS_LOG_FUNCTION (this);
}

ns3penv::DataContainer
OpenGymDiscreteContainer::GetDataContainerPbMsg()
{
    ns3penv::DataContainer dataMsg;
    ns3penv::DiscreteDataContainer* discreteMsg = dataMsg.mutable_discrete();
    discreteMsg->set_data(GetValue());
    return dataMsg;
}

bool
OpenGymDiscreteContainer::SetValue(uint32_t value)
{
    m_value = value;
    return true;
}

uint32_t
OpenGymDiscreteContainer::GetValue() const
{
    return m_value;
}

void
OpenGymDiscreteContainer::Print(std::ostream& where) const
{
    where << std::to_string(m_value);
}

template <typename T>
TypeId
OpenGymBoxContainer<T>::GetTypeId()
{
    std::string name = TypeNameGet<T>();
    static TypeId tid = TypeId("ns3::OpenGymBoxContainer<" + name + ">")
                            .SetParent<Object>()
                            .SetGroupName("OpenGym")
                            .template AddConstructor<OpenGymBoxContainer<T>>();
    return tid;
}

template <typename T>
OpenGymBoxContainer<T>::OpenGymBoxContainer()
{
    SetDtype();
}

template <typename T>
OpenGymBoxContainer<T>::OpenGymBoxContainer(std::vector<uint32_t> shape)
    : m_shape(shape)
{
    SetDtype();
}

template <typename T>
OpenGymBoxContainer<T>::~OpenGymBoxContainer()
{
}

template <typename T>
void
OpenGymBoxContainer<T>::SetDtype()
{
    std::string name = TypeNameGet<T>();
    if (name == "int8_t" || name == "int16_t" || name == "int32_t" || name == "int64_t")
    {
        m_dtype = ns3penv::INT;
    }
    else if (name == "uint8_t" || name == "uint16_t" || name == "uint32_t" || name == "uint64_t")
    {
        m_dtype = ns3penv::UINT;
    }
    else if (name == "float")
    {
        m_dtype = ns3penv::FLOAT;
    }
    else if (name == "double")
    {
        m_dtype = ns3penv::DOUBLE;
    }
    else
    {
        NS_ABORT_MSG("Unsupported data type: " + name);
    }
}

template <typename T>
void
OpenGymBoxContainer<T>::DoDispose()
{
}

template <typename T>
void
OpenGymBoxContainer<T>::DoInitialize()
{
}

template <typename T>
ns3penv::DataContainer
OpenGymBoxContainer<T>::GetDataContainerPbMsg()
{
    ns3penv::DataContainer dataMsg;
    ns3penv::BoxDataContainer* boxMsg = dataMsg.mutable_box();

    std::vector<uint32_t> shape = GetShape();
    *boxMsg->mutable_shape() = {shape.begin(), shape.end()};

    boxMsg->set_dtype(m_dtype);
    std::vector<T> data = GetData();

    switch (m_dtype)
    {
    case ns3penv::INT: {
        *boxMsg->mutable_intdata() = {data.begin(), data.end()};
        break;
    }
    case ns3penv::UINT: {
        *boxMsg->mutable_uintdata() = {data.begin(), data.end()};
        break;
    }
    case ns3penv::FLOAT: {
        *boxMsg->mutable_floatdata() = {data.begin(), data.end()};
        break;
    }
    case ns3penv::DOUBLE: {
        *boxMsg->mutable_doubledata() = {data.begin(), data.end()};
        break;
    }
    default: {
        NS_ABORT_MSG("Unsupported data type.");
    }
    }

    return dataMsg;
}

template <typename T>
bool
OpenGymBoxContainer<T>::AddValue(T value)
{
    m_data.push_back(value);
    return true;
}

template <typename T>
T
OpenGymBoxContainer<T>::GetValue(uint32_t idx)
{
    T data = 0;
    if (idx < m_data.size())
    {
        data = m_data.at(idx);
    }
    return data;
}

template <typename T>
bool
OpenGymBoxContainer<T>::SetData(std::vector<T> data)
{
    m_data = data;
    return true;
}

template <typename T>
std::vector<uint32_t>
OpenGymBoxContainer<T>::GetShape()
{
    return m_shape;
}

template <typename T>
std::vector<T>
OpenGymBoxContainer<T>::GetData()
{
    return m_data;
}

template <typename T>
void
OpenGymBoxContainer<T>::Print(std::ostream& where) const
{
    where << "[";
    for (auto i = m_data.begin(); i != m_data.end(); ++i)
    {
        where << std::to_string(*i);
        auto i2 = i;
        i2++;
        if (i2 != m_data.end())
        {
            where << ", ";
        }
    }
    where << "]";
}

TypeId
OpenGymTupleContainer::GetTypeId()
{
    static TypeId tid = TypeId("ns3::OpenGymTupleContainer")
                            .SetParent<OpenGymDataContainer>()
                            .SetGroupName("OpenGym")
                            .AddConstructor<OpenGymTupleContainer>();
    return tid;
}

OpenGymTupleContainer::OpenGymTupleContainer()
{
    // NS_LOG_FUNCTION (this);
}

OpenGymTupleContainer::~OpenGymTupleContainer()
{
    // NS_LOG_FUNCTION (this);
}

void
OpenGymTupleContainer::DoDispose()
{
    // NS_LOG_FUNCTION (this);
}

void
OpenGymTupleContainer::DoInitialize()
{
    // NS_LOG_FUNCTION (this);
}

ns3penv::DataContainer
OpenGymTupleContainer::GetDataContainerPbMsg()
{
    ns3penv::DataContainer dataMsg;
    ns3penv::TupleDataContainer* tupleMsg = dataMsg.mutable_tuple();

    for (const auto& subSpace : m_tuple)
    {
        ns3penv::DataContainer subDataContainer = subSpace->GetDataContainerPbMsg();

        tupleMsg->add_element()->CopyFrom(subDataContainer);
    }

    return dataMsg;
}

bool
OpenGymTupleContainer::Add(Ptr<OpenGymDataContainer> space)
{
    NS_LOG_FUNCTION(this);
    m_tuple.push_back(space);
    return true;
}

Ptr<OpenGymDataContainer>
OpenGymTupleContainer::Get(uint32_t idx)
{
    Ptr<OpenGymDataContainer> data;

    if (idx < m_tuple.size())
    {
        data = m_tuple.at(idx);
    }

    return data;
}

void
OpenGymTupleContainer::Print(std::ostream& where) const
{
    where << "Tuple(";

    std::vector<Ptr<OpenGymDataContainer>>::const_iterator it;
    std::vector<Ptr<OpenGymDataContainer>>::const_iterator it2;
    for (it = m_tuple.cbegin(); it != m_tuple.cend(); ++it)
    {
        Ptr<OpenGymDataContainer> subSpace = *it;
        subSpace->Print(where);

        it2 = it;
        it2++;
        if (it2 != m_tuple.end())
        {
            where << ", ";
        }
    }
    where << ")";
}

TypeId
OpenGymDictContainer::GetTypeId()
{
    static TypeId tid = TypeId("ns3::OpenGymDictContainer")
                            .SetParent<OpenGymDataContainer>()
                            .SetGroupName("OpenGym")
                            .AddConstructor<OpenGymDictContainer>();
    return tid;
}

OpenGymDictContainer::OpenGymDictContainer()
{
    NS_LOG_FUNCTION(this);
}

OpenGymDictContainer::~OpenGymDictContainer()
{
    NS_LOG_FUNCTION(this);
}

void
OpenGymDictContainer::DoDispose()
{
    NS_LOG_FUNCTION(this);
}

void
OpenGymDictContainer::DoInitialize()
{
    NS_LOG_FUNCTION(this);
}

ns3penv::DataContainer
OpenGymDictContainer::GetDataContainerPbMsg()
{
    ns3penv::DataContainer dataMsg;
    ns3penv::DictDataContainer* dictMsg = dataMsg.mutable_dict();

    for (auto& [name, subSpace] : m_dict)
    {
        ns3penv::DataContainer subDataContainer = subSpace->GetDataContainerPbMsg();
        subDataContainer.set_name(name);

        dictMsg->add_element()->CopyFrom(subDataContainer);
    }

    return dataMsg;
}

bool
OpenGymDictContainer::Add(std::string key, Ptr<OpenGymDataContainer> data)
{
    NS_LOG_FUNCTION(this);
    m_dict.insert(std::pair<std::string, Ptr<OpenGymDataContainer>>(key, data));
    return true;
}

Ptr<OpenGymDataContainer>
OpenGymDictContainer::Get(std::string key)
{
    Ptr<OpenGymDataContainer> data;
    auto it = m_dict.find(key);
    if (it != m_dict.end())
    {
        data = it->second;
    }
    return data;
}

void
OpenGymDictContainer::Print(std::ostream& where) const
{
    where << "Dict(";

    std::map<std::string, Ptr<OpenGymDataContainer>>::const_iterator it;
    std::map<std::string, Ptr<OpenGymDataContainer>>::const_iterator it2;
    for (it = m_dict.cbegin(); it != m_dict.cend(); ++it)
    {
        std::string name = it->first;
        Ptr<OpenGymDataContainer> subSpace = it->second;

        where << name << "=";
        subSpace->Print(where);

        it2 = it;
        it2++;
        if (it2 != m_dict.end())
        {
            where << ", ";
        }
    }
    where << ")";
}

// Explicit template instantiations so the library exports these symbols.
template class OpenGymBoxContainer<int32_t>;
template class OpenGymBoxContainer<uint32_t>;
template class OpenGymBoxContainer<float>;
template class OpenGymBoxContainer<double>;

} // namespace ns3
