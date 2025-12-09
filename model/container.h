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

#ifndef OPENGYM_CONTAINER_H
#define OPENGYM_CONTAINER_H

#include "messages.pb.h"

#include <ns3/object.h>
#include <ns3/type-name.h>

namespace ns3
{

/**
 * @brief Represents the container that contains the actual values
 * This is the base class
 */
class OpenGymDataContainer : public Object
{
  public:
    /** @brief constructor */
    OpenGymDataContainer();
    /** @brief destructor */
    ~OpenGymDataContainer() override;

    static TypeId GetTypeId();

    /** @brief get the protobuf message */
    virtual ns3penv::DataContainer GetDataContainerPbMsg() = 0;

    /** @brief create the container from the protobuf message */
    static Ptr<OpenGymDataContainer> CreateFromDataContainerPbMsg(
        ns3penv::DataContainer& dataContainer);

    /** @brief print the container for debugging purposes */
    virtual void Print(std::ostream& where) const = 0;

    /** @brief overload the stream operator for easy printing */
    friend std::ostream& operator<<(std::ostream& os, const Ptr<OpenGymDataContainer> container)
    {
        container->Print(os);
        return os;
    }

  protected:
    /** @brief initialize the container */
    void DoInitialize() override;
    /** @brief dispose the container */
    void DoDispose() override;
};

/** @brief Represents a container of a single discrete value */
class OpenGymDiscreteContainer : public OpenGymDataContainer
{
  public:
    /**
     * @brief the default constructor wiht n = 0
     */
    OpenGymDiscreteContainer();

    /**
     * @brief specify the maximum value
     */
    OpenGymDiscreteContainer(uint32_t n);

    /**
     * @brief default destructor
     */
    ~OpenGymDiscreteContainer() override;

    /**
     * @brief define and get the typeid
     */
    static TypeId GetTypeId();

    /**
     * @brief Read the protobuf message and get the container data
     */
    ns3penv::DataContainer GetDataContainerPbMsg() override;

    /**
     * @brief Print the container for debugging purposes
     */
    void Print(std::ostream& where) const override;

    /**
     * @brief serialize the container
     */
    friend std::ostream& operator<<(std::ostream& os, const Ptr<OpenGymDiscreteContainer> container)
    {
        container->Print(os);
        return os;
    }

    /**
     * @brief set the current value
     * @param value the integer value contained
     * @returns true if succesful else false
     */
    bool SetValue(uint32_t value);
    /**
     * @brief get the current value
     * @returns the current value
     */
    uint32_t GetValue() const;

  protected:
    // Inherited
    void DoInitialize() override;
    void DoDispose() override;

    uint32_t m_n;     // the maximum allowed value plus one
    uint32_t m_value; // the actual discrete value
};

/**
 * @brief Represents a container of a one-dimensional array of numbers
 */
template <typename T = float>
class OpenGymBoxContainer : public OpenGymDataContainer
{
  public:
    OpenGymBoxContainer();
    OpenGymBoxContainer(std::vector<uint32_t> shape);
    ~OpenGymBoxContainer() override;

    static TypeId GetTypeId();

    ns3penv::DataContainer GetDataContainerPbMsg() override;

    void Print(std::ostream& where) const override;

    friend std::ostream& operator<<(std::ostream& os, const Ptr<OpenGymBoxContainer> container)
    {
        container->Print(os);
        return os;
    }

    bool AddValue(T value);
    T GetValue(uint32_t idx);

    bool SetData(std::vector<T> data);
    std::vector<T> GetData();

    std::vector<uint32_t> GetShape();

  protected:
    // Inherited
    void DoInitialize() override;
    void DoDispose() override;

  private:
    void SetDtype();
    std::vector<uint32_t> m_shape;
    ns3penv::Dtype m_dtype;
    std::vector<T> m_data;
};

/** @brief Represents a container of a tuple of data containers */
class OpenGymTupleContainer : public OpenGymDataContainer
{
  public:
    OpenGymTupleContainer();
    ~OpenGymTupleContainer() override;

    static TypeId GetTypeId();

    ns3penv::DataContainer GetDataContainerPbMsg() override;

    void Print(std::ostream& where) const override;

    friend std::ostream& operator<<(std::ostream& os, const Ptr<OpenGymTupleContainer> container)
    {
        container->Print(os);
        return os;
    }

    bool Add(Ptr<OpenGymDataContainer> space);
    Ptr<OpenGymDataContainer> Get(uint32_t idx);

  protected:
    // Inherited
    void DoInitialize() override;
    void DoDispose() override;

    std::vector<Ptr<OpenGymDataContainer>> m_tuple;
};

/** @brief Represents a container of a dictionary of data containers */
class OpenGymDictContainer : public OpenGymDataContainer
{
  public:
    OpenGymDictContainer();
    ~OpenGymDictContainer() override;

    static TypeId GetTypeId();

    ns3penv::DataContainer GetDataContainerPbMsg() override;

    void Print(std::ostream& where) const override;

    friend std::ostream& operator<<(std::ostream& os, const Ptr<OpenGymDictContainer> container)
    {
        container->Print(os);
        return os;
    }

    bool Add(std::string key, Ptr<OpenGymDataContainer> value);
    Ptr<OpenGymDataContainer> Get(std::string key);

  protected:
    // Inherited
    void DoInitialize() override;
    void DoDispose() override;

    std::map<std::string, Ptr<OpenGymDataContainer>> m_dict;
};

} // end of namespace ns3

#endif /* OPENGYM_CONTAINER_H */
