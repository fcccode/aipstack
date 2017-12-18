/*
 * Copyright (c) 2017 Ambroz Bizjak
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef AIPSTACK_IP_UDP_PROTO_H
#define AIPSTACK_IP_UDP_PROTO_H

#include <stdint.h>
#include <stddef.h>

#include <aipstack/meta/BasicMetaUtils.h>
#include <aipstack/misc/Use.h>
#include <aipstack/misc/NonCopyable.h>
#include <aipstack/misc/Assert.h>
#include <aipstack/misc/Hints.h>
#include <aipstack/misc/MinMax.h>
#include <aipstack/structure/LinkedList.h>
#include <aipstack/structure/LinkModel.h>
#include <aipstack/structure/StructureRaiiWrapper.h>
#include <aipstack/structure/Accessor.h>
#include <aipstack/structure/LexiKeyCompare.h>
#include <aipstack/infra/Options.h>
#include <aipstack/infra/Instance.h>
#include <aipstack/infra/Err.h>
#include <aipstack/infra/Buf.h>
#include <aipstack/infra/Chksum.h>
#include <aipstack/infra/SendRetry.h>
#include <aipstack/proto/Ip4Proto.h>
#include <aipstack/proto/IpAddr.h>
#include <aipstack/proto/Udp4Proto.h>
#include <aipstack/proto/Icmp4Proto.h>
#include <aipstack/platform/PlatformFacade.h>
#include <aipstack/ip/IpIface.h>
#include <aipstack/ip/IpStackHelperTypes.h>

namespace AIpStack {

template <typename Arg>
class IpUdpProto;

template <typename UdpProto>
struct UdpListenParams {
    Ip4Addr addr = Ip4Addr::ZeroAddr();
    uint16_t port = 0;
    IpIface<typename UdpProto::TheIpStack> *iface = nullptr;
    bool accept_nonlocal_dst = false;
};

template <typename UdpProto>
struct UdpRxInfo {
    uint16_t src_port;
    uint16_t dst_port;
    bool has_checksum;
};

enum class UdpRecvResult {
    Reject,
    AcceptContinue,
    AcceptStop
};

template <typename UdpProto>
struct UdpTxInfo {
    uint16_t src_port;
    uint16_t dst_port;
};

struct UdpAssociationKey {
    Ip4Addr local_addr;
    Ip4Addr remote_addr;
    uint16_t local_port;
    uint16_t remote_port;
};

template <typename UdpProto>
struct UdpAssociationParams {
    UdpAssociationKey key;
    bool accept_nonlocal_dst = false;
};

template <typename UdpProto>
class UdpListener :
    private NonCopyable<UdpListener<UdpProto>>
{
    template <typename> friend class IpUdpProto;
    
    AIPSTACK_USE_TYPES(UdpProto, (ListenersLinkModel))

public:
    using TheIpStack = typename UdpProto::TheIpStack;

    UdpListener () :
        m_udp(nullptr)
    {}

    ~UdpListener ()
    {
        reset();
    }

    void reset ()
    {
        if (m_udp != nullptr) {
            if (m_udp->m_next_listener == this) {
                m_udp->m_next_listener = m_udp->m_listeners_list.next(*this);
            }
            m_udp->m_listeners_list.remove(*this);
            m_udp = nullptr;
        }
    }

    bool isListening () const
    {
        return m_udp != nullptr;
    }

    UdpProto & getUdp () const
    {
        AIPSTACK_ASSERT(isListening())

        return *m_udp;
    }

    UdpListenParams<UdpProto> const & getListenParams () const
    {
        AIPSTACK_ASSERT(isListening())

        return m_params;
    }

    IpErr startListening (UdpProto &udp, UdpListenParams<UdpProto> const &params)
    {
        AIPSTACK_ASSERT(!isListening())

        m_udp = &udp;
        m_params = params;
        
        m_udp->m_listeners_list.prepend(*this);

        return IpErr::SUCCESS;
    }

protected:
    virtual UdpRecvResult recvUdpIp4Packet (
        IpRxInfoIp4<TheIpStack> const &ip_info, UdpRxInfo<UdpProto> const &udp_info,
        IpBufRef udp_data) = 0;

private:
    LinkedListNode<ListenersLinkModel> m_list_node;
    UdpProto *m_udp;
    UdpListenParams<UdpProto> m_params;
};

template <typename UdpProto>
class UdpAssociation :
    private NonCopyable<UdpAssociation<UdpProto>>
{
    template <typename> friend class IpUdpProto;
    
public:
    using TheIpStack = typename UdpProto::TheIpStack;

    UdpAssociation () :
        m_udp(nullptr)
    {}

    ~UdpAssociation ()
    {
        reset();
    }

    void reset ()
    {
        if (m_udp != nullptr) {
            m_udp->m_associations_index.removeEntry(*this);
            m_udp = nullptr;
        }
    }

    bool isAssociated () const
    {
        return m_udp != nullptr;
    }

    UdpProto & getUdp () const
    {
        AIPSTACK_ASSERT(isAssociated())

        return *m_udp;
    }

    UdpAssociationParams<UdpProto> const & getAssociationParams () const
    {
        AIPSTACK_ASSERT(isAssociated())

        return m_params;
    }

    IpErr associate (UdpProto &udp, UdpAssociationParams<UdpProto> const &params)
    {
        AIPSTACK_ASSERT(!isAssociated())

        if (!udp.m_associations_index.findEntry(params.key).isNull()) {
            return IpErr::ADDR_IN_USE;
        }

        m_udp = &udp;
        m_params = params;

        m_udp->m_associations_index.addEntry(*this);

        return IpErr::SUCCESS;
    }

protected:
    virtual UdpRecvResult recvUdpIp4Packet (
        IpRxInfoIp4<TheIpStack> const &ip_info, UdpRxInfo<UdpProto> const &udp_info,
        IpBufRef udp_data) = 0;
    
private:
    typename UdpProto::AssociationIndex::Node m_index_node;
    UdpProto *m_udp;
    UdpAssociationParams<UdpProto> m_params;
};

template <typename Arg>
class IpUdpProto :
    private NonCopyable<IpUdpProto<Arg>>
{
    template <typename> friend class UdpListener;
    template <typename> friend class UdpAssociation;

    AIPSTACK_USE_VALS(Arg::Params, (UdpTTL, EphemeralPortFirst, EphemeralPortLast))
    AIPSTACK_USE_TYPES(Arg::Params, (UdpIndexService))
    AIPSTACK_USE_TYPES(Arg, (PlatformImpl))

    static_assert(EphemeralPortFirst > 0, "");
    static_assert(EphemeralPortFirst <= EphemeralPortLast, "");

    // TODO: Automatic port allocation using ephermeral port range

public:
    using TheIpStack = typename Arg::TheIpStack;

private:
    using Platform = PlatformFacade<PlatformImpl>;

public:
    using Listener = UdpListener<IpUdpProto>;
    
    using Association = UdpAssociation<IpUdpProto>;
    
private:
    struct ListenerListNodeAccessor;
    using ListenersLinkModel = PointerLinkModel<Listener>;

    using ListenersList = LinkedList<
        ListenerListNodeAccessor, ListenersLinkModel, /*WithLast=*/false>;

    struct ListenerListNodeAccessor : public MemberAccessor<
        Listener, LinkedListNode<ListenersLinkModel>, &Listener::m_list_node> {};
    
    struct AssociationIndexNodeAccessor;
    struct AssociationIndexKeyFuncs;
    using AssociationLinkModel = PointerLinkModel<Association>;
    using AssociationIndexLookupKeyArg = UdpAssociationKey const &;

    AIPSTACK_MAKE_INSTANCE(AssociationIndex, (UdpIndexService::template Index<
        AssociationIndexNodeAccessor, AssociationIndexLookupKeyArg,
        AssociationIndexKeyFuncs, AssociationLinkModel, /*Duplicates=*/false>))

    struct AssociationIndexNodeAccessor : public MemberAccessor<
        Association, typename AssociationIndex::Node, &Association::m_index_node> {};
    
    using AssociationKeyCompare = LexiKeyCompare<UdpAssociationKey, MakeTypeList<
        WrapValue<uint16_t UdpAssociationKey::*, &UdpAssociationKey::remote_port>,
        WrapValue<Ip4Addr UdpAssociationKey::*, &UdpAssociationKey::remote_addr>,
        WrapValue<uint16_t UdpAssociationKey::*, &UdpAssociationKey::local_port>,
        WrapValue<Ip4Addr UdpAssociationKey::*, &UdpAssociationKey::local_addr>
    >>;
    
    struct AssociationIndexKeyFuncs : public AssociationKeyCompare {
        inline static UdpAssociationKey const & GetKeyOfEntry (Association const &assoc)
        {
            return assoc.m_params.key;
        }
    };

public:
    static size_t const HeaderBeforeUdpData =
        TheIpStack::HeaderBeforeIp4Dgram + Udp4Header::Size;

    static size_t const MaxUdpDataLenIp4 = TypeMax<uint16_t>() - Udp4Header::Size;

    IpUdpProto (IpProtocolHandlerArgs<TheIpStack> args) :
        m_stack(args.stack),
        m_next_listener(nullptr)
    {}

    ~IpUdpProto ()
    {
        AIPSTACK_ASSERT(m_listeners_list.isEmpty())
        AIPSTACK_ASSERT(m_associations_index.isEmpty())
        AIPSTACK_ASSERT(m_next_listener == nullptr)
    }

    IpErr sendUdpIp4Packet (Ip4Addrs const &addrs, UdpTxInfo<IpUdpProto> const &udp_info,
                            IpBufRef udp_data, IpIface<TheIpStack> *iface,
                            IpSendRetryRequest *retryReq, IpSendFlags send_flags)
    {
        AIPSTACK_ASSERT(udp_data.tot_len <= MaxUdpDataLenIp4)
        AIPSTACK_ASSERT(udp_data.offset >= Ip4Header::Size + Udp4Header::Size)

        // Reveal the UDP header.
        IpBufRef dgram = udp_data.revealHeaderMust(Udp4Header::Size);

        // Write the UDP header.
        auto udp_header = Udp4Header::MakeRef(dgram.getChunkPtr());
        udp_header.set(Udp4Header::SrcPort(),  udp_info.src_port);
        udp_header.set(Udp4Header::DstPort(),  udp_info.dst_port);
        udp_header.set(Udp4Header::Length(),   dgram.tot_len);
        udp_header.set(Udp4Header::Checksum(), 0);
        
        // Calculate UDP checksum.
        IpChksumAccumulator chksum_accum;
        chksum_accum.addWords(&addrs.local_addr.data);
        chksum_accum.addWords(&addrs.remote_addr.data);
        chksum_accum.addWord(WrapType<uint16_t>(), Ip4ProtocolUdp);
        chksum_accum.addWord(WrapType<uint16_t>(), dgram.tot_len);
        uint16_t checksum = chksum_accum.getChksum(dgram);
        if (checksum == 0) {
            checksum = TypeMax<uint16_t>();
        }
        udp_header.set(Udp4Header::Checksum(), checksum);
        
        // Send the datagram.
        return m_stack->sendIp4Dgram(addrs, {UdpTTL, Ip4ProtocolUdp}, dgram,
                                     iface, retryReq, send_flags);
    }

#ifndef IN_DOXYGEN
    void recvIp4Dgram (IpRxInfoIp4<TheIpStack> const &ip_info, IpBufRef dgram)
    {
        // Check that there is a UDP header.
        if (AIPSTACK_UNLIKELY(!dgram.hasHeader(Udp4Header::Size))) {
            return;
        }
        auto udp_header = Udp4Header::MakeRef(dgram.getChunkPtr());

        // Fill in UdpRxInfo (has_checksum would be set later).
        UdpRxInfo<IpUdpProto> udp_info;
        udp_info.src_port = udp_header.get(Udp4Header::SrcPort());
        udp_info.dst_port = udp_header.get(Udp4Header::DstPort());

        // Check UDP length.
        uint16_t udp_length = udp_header.get(Udp4Header::Length());
        if (AIPSTACK_UNLIKELY(udp_length < Udp4Header::Size ||
                              udp_length > dgram.tot_len))
        {
            return;
        }
        
        // Truncate datagram to UDP length.
        dgram = dgram.subTo(udp_length);

        // Determine whether the destination address is the address of the incoming
        // network interface. By default this is a precondition for dispatching the packet
        // to associations or listeners, but those can disable this requirement.
        bool dst_is_iface_addr = ip_info.iface->ip4AddrIsLocalAddr(ip_info.dst_addr);
        
        // We will verify the checksum when we find the first matching listener.
        bool checksum_verified = false;

        // We will remember if any association or listener accepted the packet.
        bool accepted = false;

        // This lambda function is used to verify the checksum on demand.
        auto verifyChecksumOnDemand = [&]() -> bool {
            if (!checksum_verified) {
                if (!verifyChecksum(ip_info, udp_header, dgram, udp_info.has_checksum)) {
                    // Bad checksum, calling code should drop the packet.
                    return false;
                }
                checksum_verified = true;
            }
            return true;
        };

        // Check if the packet should be dispatched to a listener.
        UdpAssociationKey assoc_key =
            {ip_info.dst_addr, ip_info.src_addr, udp_info.dst_port, udp_info.src_port};
        Association *assoc = m_associations_index.findEntry(assoc_key);

        if (assoc != nullptr) do {
            AIPSTACK_ASSERT(assoc->m_udp == this)

            // Check any accept_nonlocal_dst requirement.
            if (!(assoc->m_params.accept_nonlocal_dst || dst_is_iface_addr)) {
                continue;
            }

            if (!verifyChecksumOnDemand()) {
                // Bad checksum, drop packet.
                return;
            }

            // Pass the packet to the association.
            IpBufRef udp_data = dgram.hideHeader(Udp4Header::Size);
            UdpRecvResult recv_result =
                assoc->recvUdpIp4Packet(ip_info, udp_info, udp_data);
            
            // If the association wants that we don't pass the packet to any listener, then
            // return here.
            if (recv_result == UdpRecvResult::AcceptStop) {
                return;
            }

            // Remember if the association accepted the packet.
            if (recv_result != UdpRecvResult::Reject) {
                accepted = true;
            }
        } while (false);
        
        // Look for listeners which match the incoming packet.
        // NOTE: `lis` must be properly adjusted at the end of each iteration!
        for (Listener *lis = m_listeners_list.first(); lis != nullptr;) {
            AIPSTACK_ASSERT(lis->m_udp == this)
            
            // Check if the listener matches.
            bool matches;
            {
                UdpListenParams<IpUdpProto> const &lis_params = lis->m_params;
                matches = 
                    (lis_params.port == 0 || lis_params.port == udp_info.dst_port) &&
                    (lis_params.addr.isZero() || lis_params.addr == ip_info.dst_addr) &&
                    (lis_params.iface == nullptr || lis_params.iface == ip_info.iface) &&
                    (lis_params.accept_nonlocal_dst || dst_is_iface_addr);
            }
            
            // Skip non-matching listeners.
            if (!matches) {
                lis = m_listeners_list.next(*lis);
                continue;
            }

            if (!verifyChecksumOnDemand()) {
                // Bad checksum, drop packet.
                return;
            }

            // Set the m_next_listener pointer to the next listener on the list (if any).
            // In case the following callback resets (or destructs) the next listener,
            // UdpListener::reset() will advance m_next_listener so that we can safely
            // continue iterating.
            AIPSTACK_ASSERT(m_next_listener == nullptr)
            m_next_listener = m_listeners_list.next(*lis);

            // Pass the packet to the listener.
            IpBufRef udp_data = dgram.hideHeader(Udp4Header::Size);
            UdpRecvResult recv_result =
                lis->recvUdpIp4Packet(ip_info, udp_info, udp_data);

            // Update `lis` to the next listener (if any) and clear m_next_listener.
            lis = m_next_listener;
            m_next_listener = nullptr;

            // If the listener wants that we don't pass the packet to any further listener,
            // then return here.
            if (recv_result == UdpRecvResult::AcceptStop) {
                return;
            }

            // Remember if the listener accepted the packet.
            if (recv_result != UdpRecvResult::Reject) {
                accepted = true;                
            }
        }

        // If no association or listener has accepted the datagram and it is for our IP
        // address, we should send an ICMP message.
        if (!accepted && dst_is_iface_addr) {
            if (!verifyChecksumOnDemand()) {
                // Bad checksum, drop packet.
                return;
            }

            // Send an ICMP Destination Unreachable, Port Unreachable message.
            Ip4DestUnreachMeta du_meta = {Icmp4CodeDestUnreachPortUnreach, Icmp4RestType()};
            m_stack->sendIp4DestUnreach(ip_info, dgram, du_meta);
        }
    }

    void handleIp4DestUnreach (
        Ip4DestUnreachMeta const &du_meta, IpRxInfoIp4<TheIpStack> const &ip_info,
        IpBufRef dgram_initial)
    {}
#endif

private:
    static bool verifyChecksum (
        IpRxInfoIp4<TheIpStack> const &ip_info, Udp4Header::Ref udp_header,
        IpBufRef dgram, bool &has_checksum)
    {
        uint16_t checksum = udp_header.get(Udp4Header::Checksum());

        has_checksum = (checksum != 0);

        if (has_checksum) {
            IpChksumAccumulator chksum_accum;
            chksum_accum.addWords(&ip_info.src_addr.data);
            chksum_accum.addWords(&ip_info.dst_addr.data);
            chksum_accum.addWord(WrapType<uint16_t>(), Ip4ProtocolUdp);
            chksum_accum.addWord(WrapType<uint16_t>(), dgram.tot_len);

            if (chksum_accum.getChksum(dgram) != 0) {
                return false;
            }
        }

        return true;
    }

private:
    TheIpStack *m_stack;
    StructureRaiiWrapper<ListenersList> m_listeners_list;
    StructureRaiiWrapper<typename AssociationIndex::Index> m_associations_index;
    Listener *m_next_listener;
};

struct IpUdpProtoOptions {
    AIPSTACK_OPTION_DECL_VALUE(UdpTTL, uint8_t, 64)
    AIPSTACK_OPTION_DECL_VALUE(EphemeralPortFirst, uint16_t, 49152)
    AIPSTACK_OPTION_DECL_VALUE(EphemeralPortLast, uint16_t, 65535)
    AIPSTACK_OPTION_DECL_TYPE(UdpIndexService, void)
};

template <typename... Options>
class IpUdpProtoService {
    template <typename>
    friend class IpUdpProto;
    
    AIPSTACK_OPTION_CONFIG_VALUE(IpUdpProtoOptions, UdpTTL)
    AIPSTACK_OPTION_CONFIG_VALUE(IpUdpProtoOptions, EphemeralPortFirst)
    AIPSTACK_OPTION_CONFIG_VALUE(IpUdpProtoOptions, EphemeralPortLast)
    AIPSTACK_OPTION_CONFIG_TYPE(IpUdpProtoOptions, UdpIndexService)
    
public:
    // This tells IpStack which IP protocol we receive packets for.
    using IpProtocolNumber = WrapValue<uint8_t, Ip4ProtocolUdp>;
    
#ifndef IN_DOXYGEN
    template <typename PlatformImpl_, typename TheIpStack_>
    struct Compose {
        using PlatformImpl = PlatformImpl_;
        using TheIpStack = TheIpStack_;
        using Params = IpUdpProtoService;
        AIPSTACK_DEF_INSTANCE(Compose, IpUdpProto)
    };
#endif
};

}

#endif